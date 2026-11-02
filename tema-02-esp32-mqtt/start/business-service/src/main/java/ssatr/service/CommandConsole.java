package ssatr.service;

import java.util.Scanner;

public class CommandConsole implements Runnable {

    private final MqttConnection mqtt;
    private final Dashboard dashboard;

    public CommandConsole(MqttConnection mqtt, Dashboard dashboard) {
        this.mqtt = mqtt;
        this.dashboard = dashboard;
    }

    @Override
    public void run() {
        Scanner scanner = new Scanner(System.in);
        printHelp();

        while (scanner.hasNextLine()) {
            String line = scanner.nextLine().trim();

            if (line.isEmpty()) continue;
            else if (line.equals("exit")) break;
            else if (line.equals("help")) printHelp();
            else if (line.equals("show")) dashboard.print();
            else sendCommand(line);
        }
    }

    private void sendCommand(String line) {
        String[] parts = line.split(" ", 2);
        if (parts.length == 2) {
            String devId = parts[0];
            String cmd = parts[1];
            // Construim topicul folosind numele setat în Config
            mqtt.publish(Config.commandTopic(devId), cmd);
        } else {
            System.out.println("Eroare: Format incorect. Folosiți: <deviceId> <COMANDĂ>");
        }
    }

    private void printHelp() {
        System.out.println("Comenzi: show | <deviceId> SET_INTERVAL <ms> | <deviceId> SET_THRESHOLD <valoare>");
        System.out.println("         <deviceId> RESET | <deviceId> STATUS | help | exit");
    }
}