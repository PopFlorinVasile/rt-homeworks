package ssatr.device;

import java.util.concurrent.BlockingQueue;

public class SporadicAlarmTask implements Runnable {

    static final long MIN_INTERARRIVAL_MS = 5000;

    private final String deviceId;
    private final MqttConnection mqtt;
    private final BlockingQueue<Double> alarmEvents;

    public SporadicAlarmTask(String deviceId, MqttConnection mqtt, BlockingQueue<Double> alarmEvents) {
        this.deviceId = deviceId;
        this.mqtt = mqtt;
        this.alarmEvents = alarmEvents;
    }

    @Override
    public void run() {
        long lastExecutionTime = 0;
        while (!Thread.currentThread().isInterrupted()) {
            try {
                Double temp = alarmEvents.take(); // Așteaptă aperiodic evenimente (Double)
                long currentTime = System.currentTimeMillis();

                if (currentTime - lastExecutionTime >= MIN_INTERARRIVAL_MS) {
                    String payload = deviceId + ";" + currentTime + ";" + temp + ";0.0"; // 0.0 e pragul pus generic aici
                    mqtt.publish(Config.alarmTopic(deviceId), payload);
                    lastExecutionTime = currentTime;
                } else {
                    System.out.println("Alarmă respinsă (prea deasă).");
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
}