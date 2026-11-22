# PRD: jf-flight-display

**Problem.** Show nearby aircraft live on an ESP32-2432S028 (320x240 ILI9341 + XPT2046 touch).

**v1 scope.** Standalone ESP32 (no companion server): queries adsb.fi directly over WiFi from `secrets.h`; one location and radius; one-plane-at-a-time "plate" screen on paper background (callsign, airline name and livery colours from the callsign prefix, type, vector silhouette by aircraft class rotated to its track, altitude, speed/track, distance, bearing); carousel through the nearest planes (auto-advance every 6 s, touch to skip; the selected plane is kept across refreshes); LDR backlight dimming; on-screen error reason when WiFi or the fetch fails. No demo mode.

**Out of scope v1.** AI-painted images, weather/news, airline logos/routes/airline names, web settings portal.

**Success criteria.** Firmware compiles in CI; the optional Go service tests pass on recorded fixtures; host-side parsing/format tests pass; on hardware a plane appears within ~20 s of boot (manual check).

**Known gaps vs the stated v1 idea.** Route (origin/destination) is not implemented (no adsbdb lookup); airline is inferred from the callsign prefix for a built-in table of ~140 operators (others show the 3-letter code). Silhouettes are generated per aircraft type for ~100 main types (class default otherwise), not per airframe (no AI painting or photos, unlike the Overhead widget).
