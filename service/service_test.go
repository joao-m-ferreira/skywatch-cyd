package main

import (
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"
)

func sample(t *testing.T) []byte {
	t.Helper()
	b, err := os.ReadFile("testdata/adsbfi_sample.json")
	if err != nil {
		t.Fatal(err)
	}
	return b
}

func TestParseUpstreamFiltersAndSorts(t *testing.T) {
	fl, err := parseUpstream(sample(t), 38.7223, -9.1393, 40, 10)
	if err != nil {
		t.Fatal(err)
	}
	if len(fl) != 3 { // ground, no-position and far flights are dropped
		t.Fatalf("got %d flights, want 3: %+v", len(fl), fl)
	}
	if fl[0].Callsign != "TAP456" || fl[1].Callsign != "RYR123" || fl[2].Callsign != "CS-DXX" {
		t.Fatalf("unexpected order/callsigns: %+v", fl)
	}
	if fl[0].DistKm >= fl[1].DistKm {
		t.Fatal("not nearest first")
	}
}

func TestParseUpstreamLimit(t *testing.T) {
	fl, _ := parseUpstream(sample(t), 38.7223, -9.1393, 40, 1)
	if len(fl) != 1 {
		t.Fatalf("limit ignored: %d", len(fl))
	}
}

func TestParseUpstreamMalformed(t *testing.T) {
	if _, err := parseUpstream([]byte("{not json"), 0, 0, 10, 5); err == nil {
		t.Fatal("expected error")
	}
}

func TestDistBearingNorth(t *testing.T) {
	km, b := distBearing(0, 0, 1, 0)
	if km < 110 || km > 112 || b != 0 {
		t.Fatalf("km=%v b=%v", km, b)
	}
}

func TestHandlerEndToEnd(t *testing.T) {
	up := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		_, _ = w.Write(sample(t))
	}))
	defer up.Close()
	s := httptest.NewServer(newServer(up.URL, "").routes())
	defer s.Close()
	resp, err := http.Get(s.URL + "/v1/flights?lat=38.7223&lon=-9.1393&radius_km=40&n=2")
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	if resp.StatusCode != 200 || !strings.Contains(string(body), `"cs":"TAP456"`) {
		t.Fatalf("status %d body %s", resp.StatusCode, body)
	}
}

func TestHandlerRejectsBadParamsAndToken(t *testing.T) {
	s := httptest.NewServer(newServer("http://127.0.0.1:1", "secret").routes())
	defer s.Close()
	r, _ := http.Get(s.URL + "/v1/flights?lat=1&lon=1")
	if r.StatusCode != 401 {
		t.Fatalf("want 401 got %d", r.StatusCode)
	}
	req, _ := http.NewRequest("GET", s.URL+"/v1/flights?lat=999&lon=1", nil)
	req.Header.Set("X-Token", "secret")
	r, _ = http.DefaultClient.Do(req)
	if r.StatusCode != 400 {
		t.Fatalf("want 400 got %d", r.StatusCode)
	}
}

func TestParseUpstreamAdsbFiAircraftKey(t *testing.T) {
	b, err := os.ReadFile("testdata/adsbfi_real_format.json")
	if err != nil {
		t.Fatal(err)
	}
	fl, err := parseUpstream(b, 38.72, -9.14, 40, 10)
	if err != nil || len(fl) != 2 || fl[0].Callsign != "EJU9046" {
		t.Fatalf("adsb.fi format not parsed: %v %+v", err, fl)
	}
}
