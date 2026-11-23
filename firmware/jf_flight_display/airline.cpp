#include "airline.h"

#include <ctype.h>
#include <string.h>

namespace jf {

static const Airline kAirlines[] = {
    {"TAP", "TAP Portugal", 120, 185, 40, 200, 16, 46},
    {"RYR", "Ryanair", 7, 53, 144, 241, 201, 32},
    {"EZY", "easyJet", 255, 102, 0, 40, 40, 40},
    {"SRR", "Maersk AirCargo", 70, 175, 215, 20, 40, 90},
    {"EJU", "easyJet Europe", 255, 102, 0, 40, 40, 40},
    {"BAW", "British Airways", 28, 52, 120, 200, 16, 46},
    {"AFR", "Air France", 220, 30, 50, 0, 32, 91},
    {"DLH", "Lufthansa", 5, 21, 70, 255, 173, 0},
    {"KLM", "KLM", 0, 161, 222, 0, 48, 135},
    {"IBE", "Iberia", 215, 25, 32, 255, 196, 0},
    {"VLG", "Vueling", 70, 70, 75, 255, 204, 0},
    {"UAE", "Emirates", 200, 16, 46, 180, 140, 60},
    {"QTR", "Qatar Airways", 92, 2, 49, 140, 140, 145},
    {"ETD", "Etihad", 189, 142, 44, 140, 72, 38},
    {"THY", "Turkish", 200, 16, 46, 40, 40, 40},
    {"AAL", "American", 0, 120, 190, 200, 16, 46},
    {"DAL", "Delta", 0, 40, 104, 200, 16, 46},
    {"UAL", "United", 0, 40, 110, 110, 180, 235},
    {"SWR", "Swiss", 230, 0, 0, 60, 60, 60},
    {"AUA", "Austrian", 200, 16, 46, 60, 60, 60},
    {"SAS", "SAS", 25, 0, 150, 225, 60, 30},
    {"FIN", "Finnair", 0, 32, 91, 0, 150, 200},
    {"LOT", "LOT Polish", 0, 40, 104, 200, 16, 46},
    {"ITY", "ITA Airways", 0, 60, 145, 0, 140, 70},
    {"TVF", "Transavia FR", 0, 150, 70, 30, 30, 30},
    {"TRA", "Transavia", 0, 150, 70, 30, 30, 30},
    {"WZZ", "Wizz Air", 85, 35, 135, 205, 25, 125},
    {"NSZ", "Norwegian", 210, 16, 52, 50, 50, 50},
    {"NAX", "Norwegian", 210, 16, 52, 50, 50, 50},
    {"TOM", "TUI", 110, 195, 235, 0, 50, 120},
    {"EXS", "Jet2", 220, 30, 40, 0, 50, 120},
    {"VOE", "Volotea", 220, 30, 40, 250, 250, 250},
    {"AEA", "Air Europa", 0, 135, 225, 0, 150, 210},
    {"RAM", "Royal Air Maroc", 200, 30, 40, 0, 110, 60},
    {"ELY", "El Al", 0, 50, 130, 90, 90, 90},
    {"ANA", "ANA", 0, 40, 120, 0, 130, 200},
    {"SIA", "Singapore", 0, 32, 91, 200, 160, 60},
    {"CPA", "Cathay Pacific", 0, 100, 90, 200, 16, 46},
    {"QFA", "Qantas", 230, 0, 0, 60, 60, 60},
    {"ACA", "Air Canada", 210, 20, 40, 25, 25, 25},
    {"FDX", "FedEx", 77, 20, 140, 255, 102, 0},
    {"UPS", "UPS", 75, 50, 30, 255, 185, 0},
    {"DHK", "DHL", 255, 204, 0, 212, 5, 23},
    {"BCS", "DHL", 255, 204, 0, 212, 5, 23},
    {"AEE", "Aegean", 0, 40, 120, 0, 150, 210},
    {"AIC", "Air India", 200, 30, 40, 255, 150, 0},
    {"AXB", "Air India Expr", 200, 30, 40, 255, 150, 0},
    {"AMX", "Aeromexico", 10, 40, 100, 90, 90, 90},
    {"ARG", "Aerolineas Arg", 90, 175, 230, 0, 90, 160},
    {"AZU", "Azul", 0, 40, 120, 0, 160, 230},
    {"BEL", "Brussels", 220, 40, 60, 0, 30, 90},
    {"BTI", "airBaltic", 165, 205, 50, 0, 40, 110},
    {"CLX", "Cargolux", 200, 16, 46, 0, 60, 140},
    {"CSA", "Czech Airlines", 200, 16, 46, 0, 40, 100},
    {"CTN", "Croatia Airl", 200, 16, 46, 0, 60, 140},
    {"CCA", "Air China", 200, 16, 46, 60, 60, 60},
    {"CES", "China Eastern", 0, 60, 150, 200, 16, 46},
    {"CSN", "China Southern", 0, 90, 170, 200, 16, 46},
    {"EIN", "Aer Lingus", 0, 100, 90, 160, 220, 60},
    {"ETH", "Ethiopian", 0, 120, 60, 250, 200, 0},
    {"EVA", "EVA Air", 0, 110, 70, 255, 130, 0},
    {"EWG", "Eurowings", 140, 20, 70, 150, 145, 140},
    {"GWI", "Eurowings", 140, 20, 70, 150, 145, 140},
    {"GIA", "Garuda", 0, 130, 170, 60, 60, 60},
    {"HVN", "Vietnam Airl", 0, 80, 140, 200, 160, 50},
    {"ICE", "Icelandair", 0, 60, 140, 250, 170, 0},
    {"JAL", "Japan Airlines", 200, 16, 46, 60, 60, 60},
    {"JBU", "JetBlue", 0, 40, 120, 0, 150, 220},
    {"KAL", "Korean Air", 0, 140, 200, 200, 16, 46},
    {"KLC", "KLM Cityhopper", 0, 161, 222, 0, 48, 135},
    {"HOP", "Air France Hop", 0, 32, 91, 220, 30, 50},
    {"LAN", "LATAM", 60, 30, 130, 240, 90, 100},
    {"LAT", "LATAM", 60, 30, 130, 240, 90, 100},
    {"LGL", "Luxair", 0, 170, 220, 220, 30, 50},
    {"MAS", "Malaysia", 0, 40, 110, 200, 16, 46},
    {"MSR", "EgyptAir", 0, 40, 110, 110, 180, 230},
    {"PGT", "Pegasus", 255, 190, 0, 180, 20, 50},
    {"PIA", "Pakistan Intl", 0, 100, 50, 60, 60, 60},
    {"ROT", "TAROM", 0, 60, 140, 220, 170, 0},
    {"RJA", "Royal Jordan", 35, 70, 65, 190, 160, 60},
    {"SAA", "South African", 0, 90, 150, 250, 170, 0},
    {"SKW", "SkyWest", 0, 40, 110, 110, 180, 235},
    {"SVA", "Saudia", 0, 55, 120, 190, 160, 70},
    {"SWA", "Southwest", 48, 76, 180, 255, 180, 0},
    {"SXS", "SunExpress", 0, 80, 160, 240, 120, 20},
    {"TAR", "Tunisair", 200, 16, 46, 60, 60, 60},
    {"AVA", "Avianca", 200, 16, 46, 255, 150, 0},
    {"TSC", "Air Transat", 0, 165, 220, 10, 40, 90},
    {"TUI", "TUI fly", 110, 195, 235, 0, 50, 120},
    {"JAF", "TUI fly Belgium", 110, 195, 235, 0, 50, 120},
    {"TVS", "Smartwings", 200, 20, 45, 60, 60, 70},
    {"VIR", "Virgin Atlantic", 205, 20, 50, 100, 20, 80},
    {"VOZ", "Virgin Austral", 200, 16, 46, 60, 60, 60},
    {"WJA", "WestJet", 0, 150, 210, 0, 70, 110},
    {"LDM", "Lauda Europe", 200, 16, 46, 60, 60, 60},
    {"ASL", "Air Serbia", 0, 60, 130, 200, 16, 46},
    {"BCY", "CityJet", 0, 40, 100, 200, 16, 46},
    {"CFE", "BA CityFlyer", 28, 52, 120, 200, 16, 46},
    {"EDW", "Edelweiss", 200, 16, 46, 60, 60, 60},
    {"OAW", "Helvetic", 200, 16, 46, 60, 60, 60},
    {"NJE", "NetJets Europe", 0, 40, 80, 200, 170, 60},
    {"PGA", "Portugalia", 0, 90, 60, 200, 16, 46},
    {"RZO", "Azores Airl.", 0, 130, 150, 150, 200, 230},
    {"HFY", "Hi Fly", 0, 90, 160, 200, 16, 46},
    {"RUK", "Ryanair UK", 7, 53, 144, 241, 201, 32},
    {"RYS", "Ryanair Sun", 7, 53, 144, 241, 201, 32},
    {"EZS", "easyJet Swiss", 255, 102, 0, 40, 40, 40},
    {"LOG", "Loganair", 0, 50, 120, 200, 16, 46},
    {"GEC", "LH Cargo", 255, 173, 0, 5, 21, 70},
    {"CLH", "LH CityLine", 5, 21, 70, 255, 173, 0},
    {"DLA", "Air Dolomiti", 0, 150, 165, 5, 21, 70},
    {"BOX", "AeroLogic", 255, 200, 0, 212, 5, 17},
    {"GTI", "Atlas Air", 0, 60, 130, 200, 16, 46},
    {"MPH", "Martinair", 0, 60, 140, 200, 16, 46},
    {"AFL", "Aeroflot", 0, 60, 140, 200, 16, 46},
    {"RPA", "Republic", 0, 120, 190, 200, 16, 46},
    {"ASA", "Alaska", 0, 60, 120, 0, 150, 100},
    {"FFT", "Frontier", 0, 110, 60, 250, 200, 0},
    {"NKS", "Spirit", 255, 220, 0, 40, 40, 40},
    {"ENY", "American Eagle", 0, 120, 190, 200, 16, 46},
    {"AAR", "Asiana", 200, 40, 60, 250, 190, 0},
    {"THA", "Thai", 80, 20, 110, 220, 170, 50},
    {"PAL", "Philippine", 0, 60, 150, 200, 16, 46},
    {"CEB", "Cebu Pacific", 255, 210, 0, 0, 60, 150},
    {"AXM", "AirAsia", 215, 25, 40, 60, 60, 60},
    {"LNI", "Lion Air", 200, 16, 46, 60, 60, 60},
    {"IGO", "IndiGo", 0, 40, 140, 130, 200, 250},
    {"SEJ", "SpiceJet", 230, 20, 40, 250, 200, 0},
    {"GFA", "Gulf Air", 180, 140, 70, 150, 20, 40},
    {"KAC", "Kuwait Airways", 0, 90, 160, 180, 150, 60},
    {"OMA", "Oman Air", 0, 115, 190, 0, 150, 150},
    {"MEA", "Middle East", 0, 100, 50, 200, 16, 46},
    {"FDB", "flydubai", 240, 70, 40, 0, 40, 100},
    {"ABY", "Air Arabia", 200, 16, 46, 60, 60, 60},
    {"JZR", "Jazeera", 0, 150, 220, 0, 60, 140},
    {"DHX", "DHL", 255, 204, 0, 212, 5, 23},
    {"DTA", "TAAG Angola", 200, 16, 46, 240, 200, 0},
    {"CVA", "Cabo Verde Air", 20, 60, 140, 250, 200, 0},
    {"SEH", "SKY express", 210, 40, 60, 0, 60, 150},
    {"CBJ", "Beijing Capital", 200, 16, 46, 210, 170, 60},
    {"JST", "Jetstar", 255, 90, 0, 170, 170, 170},
    {"QLK", "QantasLink", 220, 20, 40, 60, 60, 60},
    {"HAL", "Hawaiian", 90, 30, 130, 200, 60, 120},
    {"MAU", "Air Mauritius", 200, 16, 46, 250, 150, 0},
    {"RWD", "RwandAir", 0, 60, 150, 250, 200, 0},
    {"KQA", "Kenya Airways", 200, 16, 46, 0, 120, 60},
    {"FSR", "FlySafair", 0, 80, 170, 235, 60, 150},
    {"LNK", "Airlink", 0, 50, 130, 200, 16, 46},
    {"ATC", "Air Tanzania", 0, 120, 190, 250, 200, 0},
    {"DAH", "Air Algerie", 200, 16, 46, 0, 120, 60},
    {"LAM", "LAM Mozambique", 210, 30, 40, 30, 30, 30},
    {"UGD", "Uganda Airlines", 250, 200, 0, 20, 20, 20},
    {"SEY", "Air Seychelles", 0, 120, 190, 230, 60, 60},
    {"SJX", "Starlux", 190, 150, 70, 30, 30, 30},
    {"CAL", "China Airlines", 200, 30, 70, 0, 60, 130},
    {"CRK", "HK Airlines", 200, 20, 40, 250, 130, 0},
    {"AMU", "Air Macau", 0, 60, 150, 200, 30, 60},
    {"ANZ", "Air New Zealand", 25, 25, 28, 190, 190, 195},
    {"ANE", "Air Nostrum", 215, 25, 32, 255, 196, 0},
    {"SWT", "Swiftair", 0, 70, 160, 200, 16, 46},
    {"IBS", "Iberia Express", 215, 25, 32, 255, 196, 0},
    {"AMC", "Air Malta", 200, 16, 46, 250, 250, 250},
    {"LZB", "Bulgaria Air", 0, 140, 70, 200, 16, 46},
    {"AZA", "Alitalia", 0, 120, 60, 200, 16, 46},
    {"BRU", "Belavia", 0, 70, 160, 200, 16, 46},
    {"AUI", "Ukraine Intl", 0, 87, 184, 255, 215, 0},
    {"ADR", "Adria Airways", 0, 150, 170, 0, 40, 100},
};

// Approximate, recognisable liveries (tail design, fuselage scheme, engine colour, white motif).
struct LiveryRow {
  const char* icao;
  Livery livery;
};
static const LiveryRow kLiveries[] = {
    {"RYR", {kTailDisc, kBodyBelly, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"RUK", {kTailDisc, kBodyBelly, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"RYS", {kTailDisc, kBodyBelly, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"EZY", {kTailSolid, kBodyEasy, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"SRR", {kTailDisc, kBodyMaersk, 1, 1, kFinPrimary, kHullWhite, 0}},
    {"EJU", {kTailSolid, kBodyEasy, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"EZS", {kTailSolid, kBodyEasy, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"BAW", {kTailDiagonal, kBodyBelly, 1, 0, kFinSecondary, kHullWhite, 0}},
    {"AFR", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"DLH", {kTailDisc, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"IBE", {kTailCap, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"KLM", {kTailBand, kBodyCheat, 0, 0, kFinWhite, kHullSky, 0}},
    {"VLG", {kTailChecker, kBodyCheat, 2, 0, kFinWhite, kHullWhite, 0}},
    {"SWR", {kTailCross, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"WZZ", {kTailSplit, kBodyCheat, 2, 0, kFinPrimary, kHullWhite, 0}},
    {"UAE", {kTailFlag, kBodyEmirates, 0, 0, kFinWhite, kHullWhite, 0}},
    {"QTR", {kTailDisc, kBodyCheat, 0, 1, kFinPrimary, kHullPearl, 0}},
    {"ETD", {kTailFacet, kBodyEtihad, 0, 0, kFinWhite, kHullChampagne, 0}},
    {"THY", {kTailDisc, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"RAM", {kTailDisc, kBodyCheat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"AZU", {kTailChecker, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"LAN", {kTailDiagonal, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"LAT", {kTailDiagonal, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"AAL", {kTailStripes, kBodyPlain, 0, 0, kFinSecondary, kHullPearl, 0}},
    {"DAL", {kTailDiagonal, kBodyBelly, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"UAL", {kTailDisc, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"ACA", {kTailDisc, kBodyPlain, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"TSC", {kTailDisc, kBodyTransat, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"EIN", {kTailDisc, kBodyBelly, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"BEL", {kTailDisc, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"EWG", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"GWI", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"NSZ", {kTailCap, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 1}},
    {"NAX", {kTailCap, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 1}},
    {"SAS", {kTailSolid, kBodyPlain, 2, 0, kFinPrimary, kHullWhite, 0}},
    {"FIN", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"LOT", {kTailDisc, kBodyCheat, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"BTI", {kTailSolid, kBodyPlain, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"LGL", {kTailDiagonal, kBodyCheat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"VOE", {kTailChecker, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"RZO", {kTailDisc, kBodyCheat, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"AEA", {kTailDisc, kBodyTitle, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"JST", {kTailDisc, kBodyBelly, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"VOZ", {kTailSolid, kBodyPlain, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"QFA", {kTailDisc, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"JAL", {kTailDisc, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"TAP", {kTailTap, kBodyPlain, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"CPA", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"KAL", {kTailDisc, kBodyCheat, 0, 1, kFinPrimary, kHullSky, 0}},
    {"DTA", {kTailDisc, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"CVA", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"SEH", {kTailStripes, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"CBJ", {kTailDiagonal, kBodyCheat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"DHX", {kTailSolid, kBodyFull, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"BCS", {kTailSolid, kBodyFull, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"DHK", {kTailSolid, kBodyFull, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"QLK", {kTailSolid, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"SWT", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"JAF", {kTailDisc, kBodyBelly, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"TUI", {kTailDisc, kBodyBelly, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"TOM", {kTailDisc, kBodyBelly, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"ITY", {kTailStripes, kBodyFull, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"FDX", {kTailSolid, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"CLX", {kTailDisc, kBodyCheat, 0, 1, kFinPrimary, kHullPearl, 0}},
    {"AVA", {kTailDiagonal, kBodyPlain, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"ANE", {kTailCap, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"IBS", {kTailCap, kBodyPlain, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"THA", {kTailDiagonal, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"ANZ", {kTailDisc, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"GIA", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"SIA", {kTailDisc, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"AMU", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"CRK", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"CAL", {kTailDisc, kBodyCheat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"AAR", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"SJX", {kTailDisc, kBodyCheat, 0, 0, kFinWhite, kHullChampagne, 0}},
    {"EVA", {kTailDiagonal, kBodyBelly, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"ANA", {kTailDiagonal, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"TAR", {kTailDisc, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"SEY", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"UGD", {kTailBand, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"LAM", {kTailSolid, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"DAH", {kTailDisc, kBodyPlain, 1, 0, kFinWhite, kHullWhite, 0}},
    {"ATC", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"LNK", {kTailBand, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"FSR", {kTailSplit, kBodyCheat, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"KQA", {kTailStripes, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"MSR", {kTailDisc, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"SAA", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"RWD", {kTailDisc, kBodyPlain, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"MAU", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"ETH", {kTailStripes, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"NKS", {kTailSolid, kBodyFull, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"SWA", {kTailStripes, kBodyFull, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"ASA", {kTailDisc, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"HAL", {kTailDisc, kBodyPlain, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"JBU", {kTailChecker, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"AUA", {kTailDiagonal, kBodyPlain, 1, 1, kFinPrimary, kHullWhite, 0}},
    {"CTN", {kTailChecker, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"AFL", {kTailDisc, kBodyBelly, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"ASL", {kTailSolid, kBodyPlain, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"ICE", {kTailBand, kBodyPlain, 0, 0, kFinSecondary, kHullWhite, 0}},
    {"AMC", {kTailCross, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"LZB", {kTailStripes, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"AZA", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    {"BRU", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"AUI", {kTailDiagonal, kBodyPlain, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"ADR", {kTailDiagonal, kBodyPlain, 0, 0, kFinWhite, kHullWhite, 0}},
    // Corrected against reference photos (references/photos): colours, engines, fin and fuselage paint.
    {"AEE", {kTailBand, kBodyCheat, 1, 1, kFinPrimary, kHullWhite, 0}},
    {"AIC", {kTailBand, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"AMX", {kTailBand, kBodyCheat, 1, 1, kFinPrimary, kHullWhite, 0}},
    {"ABY", {kTailDiagonal, kBodyTitle, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"AXB", {kTailBand, kBodyTransat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"AXM", {kTailDisc, kBodyFull, 1, 1, kFinPrimary, kHullWhite, 0}},
    {"BOX", {kTailBand, kBodyFull, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"CCA", {kTailDisc, kBodyCheat, 0, 0, kFinWhite, kHullPearl, 0}},
    {"EDW", {kTailBand, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 1}},
    {"EXS", {kTailSolid, kBodyCheat, 1, 1, kFinPrimary, kHullPearl, 0}},
    {"MEA", {kTailDisc, kBodyCheat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"RJA", {kTailSolid, kBodyFull, 0, 0, kFinPrimary, kHullWhite, 0}},
    {"SVA", {kTailDisc, kBodyCheat, 0, 0, kFinPrimary, kHullChampagne, 0}},
    {"SXS", {kTailDiagonal, kBodyCheat, 0, 0, kFinWhite, kHullWhite, 0}},
    {"TVS", {kTailDiagSplit, kBodyCheat, 0, 1, kFinPrimary, kHullWhite, 0}},
    {"VIR", {kTailBand, kBodyCheat, 1, 0, kFinPrimary, kHullPearl, 0}},
    {"RPA", {kTailStripes, kBodyPlain, 0, 0, kFinSecondary, kHullPearl, 0}},
    {"SKW", {kTailDisc, kBodyCheat, 1, 0, kFinPrimary, kHullWhite, 0}},
    {"CFE", {kTailDiagonal, kBodyBelly, 1, 0, kFinSecondary, kHullWhite, 0}},
    {"KLC", {kTailBand, kBodyCheat, 0, 0, kFinWhite, kHullSky, 0}},
};

static void rgbToHsv(int r, int g, int b, float* h, float* s, float* v) {
  const int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
  const int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  *v = static_cast<float>(mx);
  *s = mx > 0 ? static_cast<float>(mx - mn) / static_cast<float>(mx) : 0.0f;
  float hue = 0.0f;
  if (mx != mn) {
    const float d = static_cast<float>(mx - mn);
    if (mx == r) hue = 60.0f * (static_cast<float>(g - b) / d);
    else if (mx == g) hue = 60.0f * (2.0f + static_cast<float>(b - r) / d);
    else hue = 60.0f * (4.0f + static_cast<float>(r - g) / d);
    if (hue < 0.0f) hue += 360.0f;
  }
  *h = hue;
}

bool airlineHasTwoColours(const Airline& a) {
  float h1, s1, v1, h2, s2, v2;
  rgbToHsv(a.r1, a.g1, a.b1, &h1, &s1, &v1);
  rgbToHsv(a.r2, a.g2, a.b2, &h2, &s2, &v2);
  if (s2 < 0.45f || v2 < 100.0f) return false;  // grey, black or white (or too dark to see)
  float dh = h1 > h2 ? h1 - h2 : h2 - h1;
  if (dh > 180.0f) dh = 360.0f - dh;
  return dh >= 40.0f;  // a different colour, not just another shade of the same one
}

void ledBlend(const int a[3], const int b[3], unsigned long ms, int out[3]) {
  const unsigned long p = ms % 3200;
  float t;  // 0 = a, 1 = b
  if (p < 1200) t = 0.0f;
  else if (p < 1600) t = static_cast<float>(p - 1200) / 400.0f;
  else if (p < 2800) t = 1.0f;
  else t = 1.0f - static_cast<float>(p - 2800) / 400.0f;
  for (int i = 0; i < 3; ++i) out[i] = static_cast<int>(static_cast<float>(a[i]) + static_cast<float>(b[i] - a[i]) * t);
}

Livery liveryFor(const Airline* a) {
  if (a != nullptr) {
    for (const LiveryRow& r : kLiveries) {
      if (strcmp(a->icao, r.icao) == 0) return r.livery;
    }
  }
  return {kTailBand, kBodyCheat, 0, 0, kFinPrimary, kHullWhite, 0};
}

size_t liveryCount() { return sizeof(kLiveries) / sizeof(kLiveries[0]); }
const char* liveryCode(size_t i) { return kLiveries[i].icao; }

bool looksLikeAirlineCallsign(const char* cs) {
  return cs != nullptr && strlen(cs) >= 4 && isalpha(static_cast<unsigned char>(cs[0])) &&
         isalpha(static_cast<unsigned char>(cs[1])) && isalpha(static_cast<unsigned char>(cs[2])) &&
         isdigit(static_cast<unsigned char>(cs[3]));
}

size_t airlineCount() { return sizeof(kAirlines) / sizeof(kAirlines[0]); }
const Airline& airlineAt(size_t i) { return kAirlines[i]; }

const Airline* airlineFor(const char* cs) {
  if (!looksLikeAirlineCallsign(cs)) return nullptr;
  for (const Airline& a : kAirlines) {
    if (strncmp(cs, a.icao, 3) == 0) return &a;
  }
  return nullptr;
}

}  // namespace jf
