#include "airline.h"

#include <ctype.h>
#include <string.h>

namespace jf {

static const Airline kAirlines[] = {
    {"TAP", "TAP Portugal", 0, 150, 80, 220, 30, 50},
    {"RYR", "Ryanair", 7, 53, 144, 241, 201, 32},
    {"EZY", "easyJet", 255, 102, 0, 40, 40, 40},
    {"EJU", "easyJet Europe", 255, 102, 0, 40, 40, 40},
    {"BAW", "British Airways", 28, 52, 120, 200, 16, 46},
    {"AFR", "Air France", 0, 32, 91, 220, 30, 50},
    {"DLH", "Lufthansa", 5, 21, 70, 255, 173, 0},
    {"KLM", "KLM", 0, 161, 222, 0, 48, 135},
    {"IBE", "Iberia", 215, 25, 32, 255, 196, 0},
    {"VLG", "Vueling", 255, 204, 0, 80, 80, 80},
    {"UAE", "Emirates", 200, 16, 46, 180, 140, 60},
    {"QTR", "Qatar Airways", 92, 2, 49, 100, 100, 100},
    {"ETD", "Etihad", 189, 142, 44, 80, 60, 30},
    {"THY", "Turkish", 200, 16, 46, 40, 40, 40},
    {"AAL", "American", 0, 120, 190, 200, 16, 46},
    {"DAL", "Delta", 0, 40, 104, 200, 16, 46},
    {"UAL", "United", 0, 50, 150, 60, 60, 60},
    {"SWR", "Swiss", 230, 0, 0, 60, 60, 60},
    {"AUA", "Austrian", 200, 16, 46, 60, 60, 60},
    {"SAS", "SAS", 0, 0, 100, 200, 16, 46},
    {"FIN", "Finnair", 0, 32, 91, 0, 150, 200},
    {"LOT", "LOT Polish", 0, 40, 104, 200, 16, 46},
    {"ITY", "ITA Airways", 0, 60, 145, 0, 160, 180},
    {"TVF", "Transavia FR", 0, 150, 70, 30, 30, 30},
    {"TRA", "Transavia", 0, 150, 70, 30, 30, 30},
    {"WZZ", "Wizz Air", 200, 20, 120, 40, 40, 40},
    {"NSZ", "Norwegian", 210, 16, 52, 50, 50, 50},
    {"NAX", "Norwegian", 210, 16, 52, 50, 50, 50},
    {"TOM", "TUI", 210, 20, 50, 0, 50, 120},
    {"EXS", "Jet2", 220, 30, 40, 0, 50, 120},
    {"VOE", "Volotea", 255, 90, 0, 50, 50, 50},
    {"AEA", "Air Europa", 0, 90, 170, 0, 150, 210},
    {"RAM", "Royal Air Maroc", 200, 30, 40, 0, 110, 60},
    {"ELY", "El Al", 0, 50, 130, 90, 90, 90},
    {"ANA", "ANA", 0, 40, 120, 0, 130, 200},
    {"SIA", "Singapore", 0, 32, 91, 200, 160, 60},
    {"CPA", "Cathay Pacific", 0, 100, 90, 200, 16, 46},
    {"QFA", "Qantas", 230, 0, 0, 60, 60, 60},
    {"ACA", "Air Canada", 210, 20, 40, 60, 60, 60},
    {"FDX", "FedEx", 77, 20, 140, 255, 102, 0},
    {"UPS", "UPS", 75, 50, 30, 255, 185, 0},
    {"DHK", "DHL", 255, 204, 0, 212, 5, 23},
    {"BCS", "DHL", 255, 204, 0, 212, 5, 23},
};

const Airline* airlineFor(const char* cs) {
  if (cs == nullptr || strlen(cs) < 4) return nullptr;
  if (!isalpha(static_cast<unsigned char>(cs[0])) || !isalpha(static_cast<unsigned char>(cs[1])) ||
      !isalpha(static_cast<unsigned char>(cs[2])) || !isdigit(static_cast<unsigned char>(cs[3])))
    return nullptr;
  for (const Airline& a : kAirlines) {
    if (strncmp(cs, a.icao, 3) == 0) return &a;
  }
  return nullptr;
}

}  // namespace jf
