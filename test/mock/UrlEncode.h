// Mock UrlEncode : identité (suffisant pour les tests)
#pragma once
#include <Arduino.h>
inline String urlEncode(const String &s) { return s; }
