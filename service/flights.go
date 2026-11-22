// Package main implements a small service that turns ADS-B data into the
// compact JSON the CYD display consumes.
package main

import (
	"encoding/json"
	"fmt"
	"math"
	"sort"
	"strings"
)

// Flight is the compact record sent to the display.
type Flight struct {
	Callsign string  `json:"cs"`
	Type     string  `json:"ty,omitempty"`
	Reg      string  `json:"rg,omitempty"`
	AltFt    int     `json:"alt"`
	SpeedKt  int     `json:"spd"`
	Track    int     `json:"trk"`
	DistKm   float64 `json:"km"`
	Bearing  int     `json:"brg"`
	Route    string  `json:"rt,omitempty"`
}

type upstream struct {
	AC []struct {
		Flight  string          `json:"flight"`
		Type    string          `json:"t"`
		Reg     string          `json:"r"`
		Lat     *float64        `json:"lat"`
		Lon     *float64        `json:"lon"`
		AltBaro json.RawMessage `json:"alt_baro"`
		GS      float64         `json:"gs"`
		Track   float64         `json:"track"`
	} `json:"ac"`
}

const earthKm = 6371.0

func rad(d float64) float64 { return d * math.Pi / 180 }

// distBearing returns great-circle km and initial bearing in degrees.
func distBearing(lat1, lon1, lat2, lon2 float64) (float64, int) {
	dLat, dLon := rad(lat2-lat1), rad(lon2-lon1)
	a := math.Sin(dLat/2)*math.Sin(dLat/2) +
		math.Cos(rad(lat1))*math.Cos(rad(lat2))*math.Sin(dLon/2)*math.Sin(dLon/2)
	km := 2 * earthKm * math.Asin(math.Sqrt(a))
	y := math.Sin(dLon) * math.Cos(rad(lat2))
	x := math.Cos(rad(lat1))*math.Sin(rad(lat2)) - math.Sin(rad(lat1))*math.Cos(rad(lat2))*math.Cos(dLon)
	b := math.Mod(math.Atan2(y, x)*180/math.Pi+360, 360)
	return km, int(math.Round(b)) % 360
}

// parseUpstream validates the ADS-B payload at the boundary and returns the
// n nearest airborne flights, nearest first.
func parseUpstream(body []byte, lat, lon, radiusKm float64, n int) ([]Flight, error) {
	var u upstream
	if err := json.Unmarshal(body, &u); err != nil {
		return nil, fmt.Errorf("decode upstream: %w", err)
	}
	var out []Flight
	for _, a := range u.AC {
		if a.Lat == nil || a.Lon == nil {
			continue
		}
		alt := 0
		if err := json.Unmarshal(a.AltBaro, &alt); err != nil {
			continue // "ground" or missing: not airborne
		}
		km, brg := distBearing(lat, lon, *a.Lat, *a.Lon)
		if km > radiusKm {
			continue
		}
		cs := strings.TrimSpace(a.Flight)
		if cs == "" {
			cs = strings.ToUpper(a.Reg)
		}
		out = append(out, Flight{
			Callsign: cs, Type: a.Type, Reg: a.Reg, AltFt: alt,
			SpeedKt: int(math.Round(a.GS)), Track: int(math.Round(a.Track)),
			DistKm: math.Round(km*10) / 10, Bearing: brg,
		})
	}
	sort.Slice(out, func(i, j int) bool { return out[i].DistKm < out[j].DistKm })
	if n > 0 && len(out) > n {
		out = out[:n]
	}
	return out, nil
}
