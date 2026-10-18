package ssatr.device;

import java.util.concurrent.BlockingQueue;

public class AperiodicCommandTask implements Runnable {

    private final String deviceId;
    private final TemperatureSensor sensor;
    private final MqttConnection mqtt;
    private final BlockingQueue<String> commands;
    private final PeriodicTelemetryTask periodic;

    public AperiodicCommandTask(String deviceId, TemperatureSensor sensor, MqttConnection mqtt,
                                BlockingQueue<String> commands, PeriodicTelemetryTask periodic) {
        this.deviceId = deviceId;
        this.sensor = sensor;
        this.mqtt = mqtt;
        this.commands = commands;
        this.periodic = periodic;
    }

    @Override
    public void run() {
        while (!Thread.currentThread().isInterrupted()) {
            try {
                String command = commands.take();
                String[] parts = command.split(" ");
                String result = "OK " + command;

                try {
                    switch (parts[0]) {
                        case "SET_INTERVAL":
                            periodic.setPeriodMs(Long.parseLong(parts[1]));
                            break;
                        case "SET_THRESHOLD":
                            periodic.setThreshold(Double.parseDouble(parts[1]));
                            break;
                        case "RESET":
                            sensor.reset();
                            break;
                        case "STATUS":
                            result = "OK STATUS period=" + periodic.getPeriodMs() + " threshold=" + periodic.getThreshold();
                            break;
                        default:
                            result = "ERROR Unknown command";
                    }
                } catch (Exception e) {
                    result = "ERROR Invalid arguments";
                }

                String payload = deviceId + ";" + System.currentTimeMillis() + ";" + result;
                mqtt.publish(Config.statusTopic(deviceId), payload);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
}