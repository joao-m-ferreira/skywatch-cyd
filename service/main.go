package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"sync"
	"time"
)

// version is stamped at release time via -ldflags.
var version = "dev"

const (
	defaultBase = "https://opendata.adsb.fi/api/v2"
	cacheTTL    = 15 * time.Second
)

type server struct {
	base   string
	token  string
	client *http.Client
	mu     sync.Mutex
	cache  map[string]cached
}

type cached struct {
	at   time.Time
	body []byte
}

func newServer(base, token string) *server {
	return &server{base: base, token: token, client: &http.Client{Timeout: 8 * time.Second}, cache: map[string]cached{}}
}

func qf(r *http.Request, k string, def float64) (float64, error) {
	v := r.URL.Query().Get(k)
	if v == "" {
		return def, nil
	}
	return strconv.ParseFloat(v, 64)
}

// round2 limits coordinate precision sent upstream (~1 km) for privacy.
func round2(v float64) float64 { return float64(int(v*100+0.5*sign(v))) / 100 }

func sign(v float64) float64 {
	if v < 0 {
		return -1
	}
	return 1
}

func (s *server) fetch(lat, lon, radiusKm float64) ([]byte, error) {
	nm := int(radiusKm/1.852) + 1
	url := fmt.Sprintf("%s/lat/%.2f/lon/%.2f/dist/%d", s.base, round2(lat), round2(lon), nm)
	s.mu.Lock()
	c, ok := s.cache[url]
	s.mu.Unlock()
	if ok && time.Since(c.at) < cacheTTL {
		return c.body, nil
	}
	resp, err := s.client.Get(url)
	if err == nil {
		defer resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			err = fmt.Errorf("upstream status %d", resp.StatusCode)
		}
	}
	if err != nil {
		if ok { // serve last good answer
			return c.body, nil
		}
		return nil, err
	}
	body, err := io.ReadAll(io.LimitReader(resp.Body, 4<<20))
	if err != nil {
		return nil, err
	}
	s.mu.Lock()
	s.cache[url] = cached{time.Now(), body}
	s.mu.Unlock()
	return body, nil
}

func (s *server) flights(w http.ResponseWriter, r *http.Request) {
	if s.token != "" && r.Header.Get("X-Token") != s.token {
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return
	}
	lat, e1 := qf(r, "lat", 0)
	lon, e2 := qf(r, "lon", 0)
	rad, e3 := qf(r, "radius_km", 40)
	n, e4 := qf(r, "n", 5)
	if e1 != nil || e2 != nil || e3 != nil || e4 != nil || lat < -90 || lat > 90 || lon < -180 || lon > 180 || rad <= 0 || rad > 250 {
		http.Error(w, "bad parameters", http.StatusBadRequest)
		return
	}
	body, err := s.fetch(lat, lon, rad)
	if err != nil {
		http.Error(w, "upstream unavailable", http.StatusBadGateway)
		return
	}
	fl, err := parseUpstream(body, lat, lon, rad, int(n))
	if err != nil {
		http.Error(w, "upstream malformed", http.StatusBadGateway)
		return
	}
	if fl == nil {
		fl = []Flight{}
	}
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(map[string]any{"f": fl})
}

func (s *server) routes() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("/v1/flights", s.flights)
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) { _, _ = w.Write([]byte("ok")) })
	return mux
}

func main() {
	listen := os.Getenv("LISTEN")
	if listen == "" {
		listen = ":8080"
	}
	base := os.Getenv("ADSB_BASE")
	if base == "" {
		base = defaultBase
	}
	log.Printf("flights-service %s listening on %s", version, listen)
	log.Fatal(http.ListenAndServe(listen, newServer(base, os.Getenv("API_TOKEN")).routes()))
}
