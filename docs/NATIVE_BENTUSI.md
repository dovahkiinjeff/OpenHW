# Bentusi Exchange visual

The Traders/FloatingCity canonical LOD paths now support the native Bentusi
Exchange HOD package. Activation is restricted to FloatingCity and the Traders
namespace, with a separate package header to reject cross-race manifests.

The owned source is Homeworld2.big's ship/ben_bentusiexchange model. Staging
uses the existing native HOD reader and material pipeline, calibrating scale
against the classic FloatingCity mesh. The original ship definition's [Ship]
section, including weapons and physics, is retained. Classic weapon and engine
markers remain active; no Remastered gameplay scripts are imported.

The shipglow_ns texture variant uses the same separate RGB glow/spec channels
as shipglow. Nonconstant maps retain their source resolution; uniform masks
use a single identical texel instead of an unnecessary 4K allocation.

tools/stage_bentusi_native.py creates a local staging package and audit manifest;
it does not deploy assets. Validation uses OpenHWAssetCheck --loose-ship,
the native package scope tests, and material translation tests. In-game visual
and performance acceptance requires user verification.
