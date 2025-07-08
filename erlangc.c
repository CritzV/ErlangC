#include <stdio.h>
#include <math.h>
#include <emscripten/emscripten.h>

#define MAX_ACCURACY 0.00001
#define MAX_LOOPS 100

// Utility Functions
static double minmax(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int secs(double hours) {
    return (int)(hours * 3600 + 0.5);
}

// Erlang B Formula
static double erlangB(int servers, double intensity) {
    if (servers < 0 || intensity < 0) return 0;

    double B = 1.0;
    for (int i = 1; i <= servers; i++) {
        B = (intensity * B) / (i + intensity * B);
    }
    return minmax(B, 0, 1);
}

// Erlang C Formula
static double erlangC(int servers, double intensity) {
    if (servers < 0 || intensity < 0) return 0;

    double B = erlangB(servers, intensity);
    double rho = intensity / servers;

    if (rho >= 1) return 1;

    double C = B / ((rho * B) + (1 - rho));
    return minmax(C, 0, 1);
}

// ASA Calculation
static double asa(int agents, double callsPerHour, double ahtSeconds) {
    double birthRate = callsPerHour;
    double deathRate = 3600.0 / ahtSeconds;
    double trafficRate = birthRate / deathRate;

    double utilisation = trafficRate / agents;
    if (utilisation >= 1) utilisation = 0.99;

    double C = erlangC(agents, trafficRate);
    double answerTime = C / (agents * deathRate * (1 - utilisation));

    return secs(answerTime);
}

// SLA (GOS) Calculation
static double sla(int agents, double serviceTimeSec, double callsPerHour, double ahtSeconds) {
    double birthRate = callsPerHour;
    double deathRate = 3600.0 / ahtSeconds;
    double trafficRate = birthRate / deathRate;

    double utilisation = trafficRate / agents;
    if (utilisation >= 1) utilisation = 0.99;   

    double C = erlangC(agents, trafficRate);
    double slQueued = 1 - C * exp((trafficRate - agents) * serviceTimeSec / ahtSeconds);

    return minmax(slQueued, 0, 1);
}

// Occupancy Calculation
static double occupancy(int agents, double callsIn15Min, double ahtSeconds) {
    if (agents <= 0 || callsIn15Min <= 0 || ahtSeconds <= 0) return 0;

    double totalWorkTime = callsIn15Min * ahtSeconds;
    double availableTime = agents * 15 * 60;

    return fmin(totalWorkTime / availableTime, 1);
}

// Required Agents Calculation
static double fractionalAgents(double slaTarget, double serviceTimeSec, double callsPerHour, double ahtSeconds) {
    double birthRate = callsPerHour;
    double deathRate = 3600.0 / ahtSeconds;
    double trafficRate = birthRate / deathRate;

    int agents = (int)ceil((birthRate * ahtSeconds) / 3600);
    if (agents < 1) agents = 1;

    double utilisation = trafficRate / agents;
    while (utilisation >= 1) {
        agents++;
        utilisation = trafficRate / agents;
    }

    for (int i = 0; i < MAX_LOOPS; i++) {
        double C = erlangC(agents, trafficRate);
        double slQueued = 1 - C * exp((trafficRate - agents) * serviceTimeSec / ahtSeconds);

        if (slQueued >= slaTarget || slQueued > (1 - MAX_ACCURACY)) break;

        agents++;
    }

    return agents;
}

// Wrappers for 15-minute intervals
EMSCRIPTEN_KEEPALIVE
int asa15(int agents, double callsIn15Min, double ahtSeconds) {
    double callsPerHour = callsIn15Min * 4;
    return (int)asa(agents, callsPerHour, ahtSeconds);
}

EMSCRIPTEN_KEEPALIVE
double gos15(int agents, double callsIn15Min, double serviceTimeSec, double ahtSeconds) {
    double callsPerHour = callsIn15Min * 4;
    return sla(agents, serviceTimeSec, callsPerHour, ahtSeconds);
}

EMSCRIPTEN_KEEPALIVE
double occupancy15(int agents, double callsIn15Min, double ahtSeconds) {
    return occupancy(agents, callsIn15Min, ahtSeconds);
}

EMSCRIPTEN_KEEPALIVE
int requiredAgents15(double callsIn15Min, double ahtSeconds, double slaTarget, double serviceTimeSec) {
    double callsPerHour = callsIn15Min * 4;
    return (int)ceil(fractionalAgents(slaTarget, serviceTimeSec, callsPerHour, ahtSeconds));
}

int main() {
    return 0;
}
