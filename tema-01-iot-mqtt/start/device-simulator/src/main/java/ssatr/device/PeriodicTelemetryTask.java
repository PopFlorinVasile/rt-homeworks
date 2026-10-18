package ssatr.device;

import java.util.concurrent.BlockingQueue;

public class PeriodicTelemetryTask implements Runnable {

    private final String deviceId;
    private final TemperatureSensor sensor;
    private final MqttConnection mqtt;
    private final BlockingQueue<Double> alarmEvents;

    private volatile long periodMs = 2000;
    private volatile double threshold = 28.0;

    public PeriodicTelemetryTask(String deviceId, TemperatureSensor sensor, MqttConnection mqtt, BlockingQueue<Double> alarmEvents) {
        this.deviceId = deviceId;
        this.sensor = sensor;
        this.mqtt = mqtt;
        this.alarmEvents = alarmEvents;
    }

    @Override
    public void run() {
        long nextReleaseTime = System.currentTimeMillis();
        while (!Thread.currentThread().isInterrupted()) {
            try {
                long sleepTime = nextReleaseTime - System.currentTimeMillis();
                if (sleepTime > 0) Thread.sleep(sleepTime);

                double temp = sensor.read();
                String payload = deviceId + ";" + System.currentTimeMillis() + ";" + temp;
                mqtt.publish(Config.telemetryTopic(deviceId), payload);

                if (temp > threshold) {
                    alarmEvents.offer(temp); // Adăugăm direct valoarea Double
                }
                nextReleaseTime += periodMs;
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    public void setPeriodMs(long periodMs) { this.periodMs = periodMs; }
    public void setThreshold(double threshold) { this.threshold = threshold; }
    public long getPeriodMs() { return periodMs; }
    public double getThreshold() { return threshold; }
}