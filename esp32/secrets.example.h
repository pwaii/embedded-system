// Copy to secrets.h (git-ignored) and fill in. Never commit secrets.h.
#pragma once

// Primary + fallback networks (fallback = phone hotspot for demo day)
#define WIFI_SSID_1      "your-wifi"
#define WIFI_PASS_1      "your-password"
#define WIFI_SSID_2      "demo-hotspot"
#define WIFI_PASS_2      "hotspot-password"

// Firebase project settings (Project settings → General → Web app)
#define FIREBASE_API_KEY "AIza..."
#define FIREBASE_DB_URL  "https://<project>-default-rtdb.asia-southeast1.firebasedatabase.app"
