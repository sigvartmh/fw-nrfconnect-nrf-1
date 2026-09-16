# Scope-bound release for the CRACEN PK request (`__cleanup`)

## Context

`sx_pk_acquire_hw(&req)` / `sx_pk_release_req(&req)` is a manual acquire/release pair that every
caller has to get right on every exit path. Across `subsys/nrf_security/src/drivers/cracen/` there
are **51 acquire sites carrying 104 explicit release calls** — roughly two releases per acquire,
because each error path repeats the release. That duplication is where the bugs live: a recent fix
(`632f4df867`) removed a double-release class caused by callees releasing too, and a survey for
this plan found **5 sites that still leak the request** on some path, holding
`cracen_mutex_asymmetric` and an un-decremented CRACEN refcount forever:

| Leak | Path |
|---|---|
| `cracen_psa_srp.c:383, :427` | bare `return status;` on hash-helper failure |
| `cracen_psa_wpa3_sae.c:244, :257, :308` | three returns inside the hunt-and-peck loop |
| `cracen_psa_wpa3_sae.c:739` | `exit:` label runs `cracen_mac_abort()` and returns, no release (verified) |
| `cracen_wpa3_key_management.c:248, :277` | bare returns |
| `cracen_rsa_key_management.c:168, :207` | acquire is the first statement, before validation |

The goal is to make release automatic at scope exit so callers cannot forget, using GCC's variable
`cleanup` attribute. **Decision taken: true RAII, GNU-only** — explicit releases are deleted and
correctness depends on the attribute, with a hard compile-time error on a toolchain that lacks it.

### Prerequisite: get an explicit ruling on IAR

This is a knowing trade, not an oversight, and it needs sign-off before the code lands:

- Zephyr's `CONFIG_TOOLCHAIN_SUPPORTS_VARIABLE_CLEANUP_ATTRIBUTE` defaults `y`
  (`zephyr/Kconfig.zephyr:637`) but IAR sets it `n` in `zephyr/cmake/toolchain/iar/Kconfig.defconfig:8`
  — a file authored by IAR Systems AB. `toolchain/iar.h` never includes `gcc.h`, so `__cleanup` is
  undefined under iccarm.
- CRACEN has a live iccarm accommodation: the `#ifdef __IAR_SYSTEMS_ICC__` fallback at
  `cracenpsa/src/cracen_psa_ctr_drbg.c:41-49`, added 2025-08-11 by Robin Kastberg (IAR) — the only
  compiler guard in all of `nrf_security`. Someone is maintaining that build path.
- TF-M is **not** a risk: `zephyr/modules/trusted-firmware-m/CMakeLists.txt:223-233` hard-codes
  `toolchain_GNUARM.cmake` and `FATAL_ERROR`s on any other variant.
- MISRA-C 2012 Rule 1.2 ("language extensions should not be used") formally applies to new code via
  `nrf/doc/nrf/dev_model_and_contributions/contributions.rst:49` → Zephyr coding guidelines rule 17.
  Nothing in CI enforces it, so this is a code-owner review question. Using Zephyr's own sanctioned
  `__cleanup` macro rather than a bespoke one is the best available answer.

Raise it with the `nrf_security` code owners and the IAR maintainer in the PR description. If the
answer is "iccarm must keep building", fall back to the portable shape: one release at a single
`goto exit` label per function, with `__cleanup` used only as a debug assertion.

## Change 1 — Infrastructure (own commit, no call-site changes, behaviour-neutral)

**`silexpk/target/baremetal_ba414e_with_ik/pk_baremetal.c`** — make release idempotent, which is
what lets the attribute fire safely on paths that never acquired and on paths that already released:

```c
void sx_pk_release_req(sx_pk_req *req)
{
	if (req->cnx == NULL) {
		return;          /* never acquired, or already released */
	}
	... existing body ...
	req->cnx = NULL;     /* was never cleared before */
}
```

`req->cnx` is the right sentinel: `sx_pk_acquire_hw()` sets it (`pk_baremetal.c:250`) and nothing
else reads it back — acquire always assigns `req->cnx` itself, so nulling it on the singleton in
`sx_pk_init()` (`pk_baremetal.c:236`) is harmless. Verified by reading both functions.

**`silexpk/include/silexpk/core.h`** — reuse Zephyr's macro, do not spell the attribute directly:

```c
#include <zephyr/toolchain.h>            /* __cleanup(), zephyr/toolchain/gcc.h:325 */

#if !defined(CONFIG_TOOLCHAIN_SUPPORTS_VARIABLE_CLEANUP_ATTRIBUTE)
#error "The CRACEN PK driver requires the variable cleanup attribute."
#endif

/** Declare a PK request released automatically when it leaves scope. */
#define SX_PK_REQ_AUTO(_name) sx_pk_req _name __cleanup(sx_pk_release_req) = {0}
```

Three details that matter, all verified by compiling a probe with `arm-zephyr-eabi-gcc 12.2.0`
(clean under `-Wall -Wextra -Wpedantic`):

- **`sx_pk_release_req`'s existing signature works directly as the handler** — the attribute calls
  `f(&var)`, so no wrapper function is needed.
- **The `= {0}` is load-bearing, not style.** The attribute fires on *every* scope exit, including
  the many early returns that happen before the acquire — the declaration and the acquire are far
  apart at `cracen_ecdh_weierstrass.c` (decl :33, acquire :61, four returns between),
  `cracen_psa_wpa3_sae.c` (decl :348, acquire :387, four `goto exit` between),
  `cracen_psa_srp.c:474/:488`, `cracen_rsa_pkcs1v15_sign_digest` (:125/:144) and others. Zero-init
  plus the `cnx` guard makes those a no-op; without it they would release an uninitialised request.
- **Cleanup runs on `goto` out of a block, before the label**, so the `goto exit;` idiom used all
  over this driver keeps working.

Also revise the `@note` blocks I added to `core.h`/`ik.h` in `632f4df867`: they currently state
release is not idempotent, which this commit changes.

## Change 2 — Convert per bucket, not uniformly

The 51 sites are not interchangeable. Rules, in the order they should be applied:

**Bucket A — 33 sites: convert fully.** Declare with `SX_PK_REQ_AUTO`, delete every explicit
release. These have nothing but `return` and local `safe_memzero` after the last release. Includes
`cracen_ecc_keygen.c`, `cracen_ecdsa.c` (3 functions, 10 releases), `cracen_ikg_operations.c`,
`cracen_montgomery.c`, both ECDH files, `cracen_psa_jpake.c`, `cracen_psa_spake2p.c`.

**Bucket B — 12 sites: keep the early release.** These deliberately release and *then* do real
work, and collapsing that into scope-bound release would hold the PK mutex across it:
`cracen_rsa_oaep_decrypt` (two MGF1/SHA passes after release), `cracen_rsa_pss_verify_digest`
(whole EMSA-PSS decode), `cracen_rsa_pkcs1v15_decrypt` (padding scan), the four EdDSA sign/pubkey
functions, both RSA encrypt functions, `cracen_import_wpa3_sae_pt_key`, and the two
`psa_ext_ecc_secp160r1_*mult_base` functions. Thanks to Change 1 the explicit release simply
disarms the cleanup, which then no-ops — verified in the probe. **This is the one place I disagree
with the survey**, which said bucket B's releases must be deleted; that is only true without the
idempotence guard, and deleting them would be a regression.

Two of these matter more than the rest: `ed25519_sign_internal` and `ed448_sign_internal` hold the
*symmetric* DMA lock (`sx_hw_reserve` at `cracen_eddsa_ed25519.c:94`) across the entire PK window
and only release it at the `exit:` label. Verified by reading both. Widening the PK-held window
there worsens an existing nested-lock ordering, so their early release must stay.

**Bucket E — 5 leak sites: convert fully.** This is where the change pays for itself; each leak
disappears with no other edit. Give it its own commit so it is bisectable as a bug fix.

**Do not convert:** `sx_pk_init()` in `pk_baremetal.c:234-236` operates on the static singleton
`silex_pk_engine.instance`, not a stack local. And in the IKG paths, keep the explicit `exit_ikg()`
call — `sx_pk_release_req()` never touches `req->ik_mode`, so auto-release frees the mutex but does
*not* take the engine out of IK mode. Add a comment saying so.

Buckets C (release inside a loop) and D (request escapes the acquiring function) are **empty** — the
survey found no nested acquisition and no callee that releases, and
`internal/rsa/cracen_rsa_common.h:76` already documents that contract. `sx_pk_req` is never copied,
returned by value, or stored in a struct. Scope-bound release is therefore structurally sound.

## Design as built: a Kconfig toggle, not a hard `#error`

`CRACEN_PK_SCOPE_BOUND_RELEASE` (in `src/drivers/cracen/Kconfig`) `depends on
TOOLCHAIN_SUPPORTS_VARIABLE_CLEANUP_ATTRIBUTE`, so it is simply unavailable under iccarm, and is
user-visible so the fallback can be **built and tested on GCC** — that is what keeps the IAR path
from rotting. `core.h` provides the pair:

```c
SX_PK_REQ_AUTO(req);          /* toggle on: released at scope exit */
sx_pk_acquire_hw(&req);
if (err) goto exit;
exit:
	SX_PK_REQ_DONE(req);  /* toggle on: no-op; off: the release */
```

Writing both is correct either way, so converted sites must be **single-exit**. Falling back still
releases, so a toolchain difference cannot become a leak; a no-op cleanup is deliberately not
offered. `core.h` defines `__cleanup` itself when `<zephyr/toolchain.h>` is absent, so the header no
longer requires Zephyr. checkpatch reads the declaration macro as a statement, so it goes last in
the declaration block with a blank line before it.

## PR description (paste-ready)

> ### CRACEN: scope-bound release for the public-key request
>
> The PK request is acquired and released by hand at 51 sites carrying 104 release calls, about two
> per acquisition because each error path repeats it. Five of those sites leaked the request
> outright, holding `cracen_mutex_asymmetric` and an un-decremented CRACEN refcount for the lifetime
> of the image. This series fixes the leaks, then makes the release scope-bound so the next one
> cannot be written.
>
> **Read it in three groups.** Commits 1-5 are standalone portable leak fixes, deliberately ahead of
> everything else so they can be cherry-picked to a release branch and do not depend on the compiler
> extension. Commit 6 is the infrastructure. The rest convert call sites, one file per commit, no
> behaviour change intended.
>
> **A note on what `SX_PK_REQ_DONE()` does.** With `CRACEN_PK_SCOPE_BOUND_RELEASE=y` (the default on
> GCC) it expands to nothing and the cleanup attribute performs the release at scope exit; with the
> option off it *is* the release. So on a GCC build the release point moves from where
> `SX_PK_REQ_DONE()` sits to the end of the enclosing scope. Where only a small copy or zeroize
> follows, that is what the converted sites do and the cost is negligible. Where it would matter the
> conversions instead keep a literal `sx_pk_release_req()` call, which the idempotence guard turns
> the cleanup into a no-op for:
> - both EdDSA sign paths, which also hold the symmetric hardware reservation and must release the
>   PK engine before it;
> - the RSA paths that run MGF1/SHA or a full padding decode after releasing.
>
> Some earlier commit messages in this series say the release "stays where it was", which is precise
> for the portable path and loose for the GCC path in exactly the small-copy cases above.
>
> **Please rule on IAR before merging.** `CRACEN_PK_SCOPE_BOUND_RELEASE` depends on Zephyr's
> `TOOLCHAIN_SUPPORTS_VARIABLE_CLEANUP_ATTRIBUTE`, which `zephyr/cmake/toolchain/iar/Kconfig.defconfig`
> sets to `n`, so iccarm builds get the fallback and remain correct. That is deliberate: this driver
> already carries an iccarm accommodation (`cracen_psa_ctr_drbg.c`, `ALIGN_ON_STACK`, added by
> @RobinKastberg in Aug 2025). The option is user-visible specifically so the fallback path is built
> and tested on GCC rather than left to rot. Also note MISRA-C 2012 Rule 1.2 (no language extensions)
> formally applies to new code via the Zephyr coding guidelines; nothing in CI enforces it, so this
> is a reviewer decision. Using Zephyr's own `__cleanup` macro rather than a bespoke one is the best
> available answer.
>
> **Verification.** Every commit builds clean with `CRACEN_PK_SCOPE_BOUND_RELEASE` both `y` and `n`.
> On nRF54L15 hardware: `tests/subsys/nrf_security/ecc_secp160r1`,
> `tests/subsys/bluetooth/fast_pair/crypto` and `samples/crypto/{ecdh,ecdsa,eddsa,ecjpake,spake2p,rsa}`
> all pass in both states. The secp160r1 and Fast Pair suites also pass on nRF54LC10 and nRF54LM20.
> SRP and WPA3-SAE have no sample or test coverage in tree, so those files are build-verified only.
> Not yet run: psa-arch-tests against the recorded baseline, which is worth doing since this touches
> every PK user.
>
> **Two separate bugs found while doing this, deliberately not fixed here.**
> `cracen_wpa3_sae_calc_pwe_hnp()` tests `status` where it means `sx_status` after
> `cracen_ec_pt_calc_y_sqr()`; both constants are 0, so the check silently never fires.
> `cracen_ecc_is_quadratic_residue()` is unused and has no releasing owner.

## Status

Landed and verified (all builds clean in **both** toggle states; hardware runs on nRF54L15):

| Commit | Contents |
|---|---|
| `b7ceb5034c` | leak fix: SRP client S |
| `962123421c` | leak fix: WPA3-SAE hunt and peck |
| `ba6d3ba42a` | leak fix: WPA3-SAE key calc |
| `9eece4d27f` | leak fix: WPA3-SAE PT derivation |
| `90b692d503` | leak fix: RSA key generation (acquire at first use) |
| `a035d72da0` | infrastructure: idempotent release, `SX_PK_REQ_AUTO`/`SX_PK_REQ_DONE`, Kconfig |
| `377e5bc782` | convert `psa_ext_ecc_secp160r1.c` (3 sites) |
| `6ff2dc5b21` | convert ECC public key generation |
| `f221ee83b1` | convert Montgomery key generation |
| `509c8b9bfb` | convert Montgomery ECDH |
| `e3e3299000` | convert Weierstrass ECDH |
| `ebed61eb8f` | convert ECDSA (10 releases → 3), plus workmem zeroization on all paths |

The five leak fixes are portable and come **before** the infrastructure, so each is cherry-pickable
to a release branch and none depends on the cleanup attribute.

### Remaining conversions, roughly 40 sites in 11 files

Straightforward (bucket A, single-exit already or nearly):
`internal/ecc/cracen_ecc_helpers.c`, `cracen_psa_jpake.c`, `cracen_psa_spake2p.c`,
`cracen_psa_srp.c`, `internal/pake/cracen_wpa3_key_management.c`,
`internal/rsa/cracen_rsa_coprime_check.c`, `internal/rsa/cracen_rsa_key_management.c`.

Need care — keep the early release, do **not** widen the held window:
- `internal/ecc/cracen_eddsa_ed25519.c` and `_ed448.c`: both hold the *symmetric* DMA lock
  (`sx_hw_reserve`) across the whole PK window and release it at `exit:`. Widening the PK window
  worsens that nested-lock ordering.
- `internal/rsa/cracen_rsa_encryption_rsaes_oaep.c`, `_rsaes_pkcs1v15.c`,
  `internal/rsa/cracen_rsa_signature_rsapss.c`: each runs MGF1/SHA or full padding decode after
  releasing.
- `cracen_psa_wpa3_sae.c`: releases before the constant-time selection.
- `internal/ikg/cracen_ikg_operations.c`: keep the explicit `exit_ikg()`; release never touches
  `req->ik_mode`, so auto-release frees the mutex but leaves the engine in IK mode.

### Also noticed, not fixed (separate bugs, deliberately out of scope)

- `cracen_psa_wpa3_sae.c` in `cracen_wpa3_sae_calc_pwe_hnp()` checks `if (status != SX_OK)` after
  `cracen_ec_pt_calc_y_sqr()` when it means `sx_status`. `status` is `PSA_SUCCESS` (0) there and
  `SX_OK` is 0, so the check never fires and a y² failure goes unnoticed. Worth its own fix.
- `cracen_ecc_helpers.c:360` `cracen_ecc_is_quadratic_residue()` is unused and has no releasing
  owner; `cracen_psa_wpa3_sae.c:306` (pre-existing) returns from inside a loop without releasing.

## Verification

- **Build matrix**, since the `#error` is the risky part: nRF54L15, nRF54LC10, nRF54LM20, plus at
  least one `*/ns` (TF-M) target to confirm `CONFIG_TOOLCHAIN_SUPPORTS_VARIABLE_CLEANUP_ATTRIBUTE`
  actually reaches the separate TF-M-side CRACEN build (`subsys/nrf_security/tfm/CMakeLists.txt`
  re-adds `src/drivers/cracen`). If it does not, widen the guard to `|| defined(__GNUC__)` rather
  than dropping the check.
- **Hardware**, all three DKs — console VCOM differs per board, see the map in memory:
  `west twister -T tests/subsys/nrf_security/ecc_secp160r1 -T tests/subsys/bluetooth/fast_pair/crypto
  --device-testing --hardware-map <map>` and the asymmetric samples
  `samples/crypto/{ecdh,ecdsa,eddsa,ecjpake,spake2p,rsa}`. All of these pass today, so any
  regression is attributable.
- **psa-arch-tests** crypto suite against the recorded 45/18/1 baseline — this change touches every
  PK user, so this is the broadest net available. (Still outstanding from the previous change too.)
- **Refcount underflow guard**: temporarily `__ASSERT(users > 0, ...)` in `cracen_release()`
  (`common/src/cracen/hardware/hardware.c:96`) while running the full matrix, to catch a
  double-release the type system cannot. `__ASSERT_NO_MSG` is already used in 9 files here.
- **Size/stack delta**: `= {0}` zero-inits ~76 bytes per acquire that were previously left
  uninitialised. Expected to be noise against a point multiplication, but record ROM/RAM before and
  after on nRF54L15 so the review has a number.
