package ssatr.service;

import java.util.concurrent.ConcurrentHashMap;
import java.util.Map;

public class Dashboard {

    // Salvăm ultima stare cunoscută
    private final Map<String, DeviceState> states = new ConcurrentHashMap<>();

    // Clasă internă pentru a ține datele
    private static class DeviceState {
        double lastTemp = 0.0;
        long lastTimestamp = 0;
        int alarmCount = 0;
    }

    public void updateTelemetry(String deviceId, double temp, long timestamp) {
        DeviceState state = states.computeIfAbsent(deviceId, k -> new DeviceState());
        state.lastTemp = temp;
        state.lastTimestamp = timestamp;
    }

    public void incrementAlarms(String deviceId) {
        DeviceState state = states.computeIfAbsent(deviceId, k -> new DeviceState());
        state.alarmCount++;
    }

    public void print() {
        System.out.println("--- Dashboard ---");
        long now = System.currentTimeMillis();
        for (Map.Entry<String, DeviceState> entry : states.entrySet()) {
            String devId = entry.getKey();
            DeviceState s = entry.getValue();
            long ageMs = now - s.lastTimestamp;
            System.out.printf("Device: %s | Temp: %.2f | Alarme: %d | Ultimul mesaj acum: %d ms%n",
                    devId, s.lastTemp, s.alarmCount, ageMs);
        }
    }
}