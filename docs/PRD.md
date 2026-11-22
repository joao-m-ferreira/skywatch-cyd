# PRD: jf-flight-display

**Problem.** Show nearby aircraft live on an ESP32-2432S028 (320x240 ILI9341 + XPT2046 touch).

**v1 scope.** WiFi from `secrets.h`; one location and radius; nearest-plane screen (callsign, type, altitude, speed/heading, distance, bearing); list of the nearest planes; touch to cycle; LDR backlight dimming; demo mode with sample data when WiFi is unavailable.

**Out of scope v1.** AI-painted images, weather/news, airline logos/routes/airline names, web settings portal.

**Success criteria.** Firmware compiles in CI; Go service tests pass on recorded fixtures; host-side parsing/format tests pass; on hardware a plane appears within ~20 s of boot (manual check).

**Known gaps vs the stated v1 idea.** Route and airline name are not implemented yet (the service has no adsbdb lookup); callsign + type are shown instead.
