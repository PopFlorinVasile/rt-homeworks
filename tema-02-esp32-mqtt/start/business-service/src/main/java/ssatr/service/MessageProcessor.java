package ssatr.service;

import java.util.concurrent.BlockingQueue;

public class MessageProcessor implements Runnable {

    private final String kind;
    private final BlockingQueue<String> inbox;
    private final CsvStorage storage;
    private final Dashboard dashboard;

    public MessageProcessor(String kind, BlockingQueue<String> inbox, CsvStorage storage, Dashboard dashboard) {
        this.kind = kind;
        this.inbox = inbox;
        this.storage = storage;
        this.dashboard = dashboard;
    }

    @Override
    public void run() {
        while (!Thread.currentThread().isInterrupted()) {
            try {
                String payload = inbox.take();
                String[] parts = payload.split(";");
                String devId = parts[0];
                long timestamp = Long.parseLong(parts[1]);

                // Modificarea este aici: folosim append() cu 2 parametri
                storage.append(kind, payload);

                if (kind.equals("telemetry")) {
                    double temp = Double.parseDouble(parts[2]);
                    dashboard.updateTelemetry(devId, temp, timestamp);
                } else if (kind.equals("alarm")) {
                    dashboard.incrementAlarms(devId);
                    System.out.println("ALARM: " + payload);
                } else if (kind.equals("status")) {
                    System.out.println("STATUS: " + payload);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Exception e) {
                System.err.println("Eroare procesare mesaj: " + e.getMessage());
            }
        }
    }
}