# Integration Fixtures

The portable integration tests use the SDL applications built from
`tests/apps`. Hardware-dependent evdev coverage is capability-gated; the unit
protocol test remains mandatory on every host.

Both fixtures include an application-side input monitor. Events that survive
the preload and reach the fixture are printed as `APP_RECEIVED` lines and
summarized as `APP_AUDIT_SUMMARY`; the same counters are shown in the SDL
window title. This makes inactive pass-through, active blocking, and generated
submission output directly observable without relying on library diagnostics.
