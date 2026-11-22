// Host-side unit tests (no Arduino). Run via scripts/test.sh.
#include <stdio.h>
#include <string.h>

#include "../jf_flight_display/board.h"
#include "../jf_flight_display/flights.h"

static int failures = 0;
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);  \
      ++failures;                                         \
    }                                                     \
  } while (0)

int main() {
  const char* ok =
      "{\"f\":[{\"cs\":\"TAP456\",\"ty\":\"A320\",\"alt\":3500,\"spd\":180,\"trk\":200,"
      "\"km\":4.2,\"brg\":225},{\"cs\":\"RYR123\",\"alt\":12000,\"km\":150.4,\"brg\":10}]}";
  jf::Flight f[jf::kMaxFlights];
  int n = jf::parseFlights(ok, strlen(ok), f, jf::kMaxFlights);
  CHECK(n == 2);
  CHECK(strcmp(f[0].callsign, "TAP456") == 0);
  CHECK(strcmp(f[0].type, "A320") == 0);
  CHECK(f[0].alt_ft == 3500 && f[0].bearing == 225);
  CHECK(f[1].type[0] == '\0');

  CHECK(jf::parseFlights("{bad", 4, f, jf::kMaxFlights) == -1);
  const char* nof = "{\"x\":1}";
  CHECK(jf::parseFlights(nof, strlen(nof), f, jf::kMaxFlights) == -1);
  const char* nocs = "{\"f\":[{\"alt\":1}]}";
  CHECK(jf::parseFlights(nocs, strlen(nocs), f, jf::kMaxFlights) == 0);
  CHECK(jf::parseFlights(ok, strlen(ok), f, 1) == 1);  // honours max

  char b[16];
  jf::formatAltitude(3500, b, sizeof b);
  CHECK(strcmp(b, "3500 ft") == 0);
  jf::formatAltitude(12000, b, sizeof b);
  CHECK(strcmp(b, "FL120") == 0);
  jf::formatDistance(4.24f, b, sizeof b);
  CHECK(strcmp(b, "4.2 km") == 0);
  jf::formatDistance(150.4f, b, sizeof b);
  CHECK(strcmp(b, "150 km") == 0);

  CHECK(strcmp(jf::compass(0), "N") == 0);
  CHECK(strcmp(jf::compass(359), "N") == 0);
  CHECK(strcmp(jf::compass(225), "SW") == 0);
  CHECK(strcmp(jf::compass(-90), "W") == 0);

  CHECK(jf::backlightFor(-5) == 20);
  CHECK(jf::backlightFor(4095) == 255);
  CHECK(jf::backlightFor(99999) == 255);

  CHECK(board::kScreenW == 320 && board::kScreenH == 240);
  CHECK(board::kTftCs == 15 && board::kTouchCs == 33);

  if (failures == 0) printf("all firmware host tests passed\n");
  return failures ? 1 : 0;
}
