/*****************************************************
*
* File:  videoStreamTest.cc
*
* Explanation:  This script modifies the tutorial first.cc
*               to test the video stream application.
*
*****************************************************/
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/gnuplot.h" 
#include "ns3/yans-wifi-helper.h"



using namespace ns3;

//#define NS3_LOG_ENABLE

/**
 * @brief The test cases include:
 * 1. P2P network with 1 server and 1 client
 * 2. P2P network with 1 server and 2 clients
 * 3. Wireless network with 1 server and 3 mobile clients
 * 4. Wireless network with 3 servers and 3 mobile clients
 */
 
 //Testing Bash File
#define CASE 6

NS_LOG_COMPONENT_DEFINE ("VideoStreamTest");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("VideoStreamClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("VideoStreamServerApplication", LOG_LEVEL_INFO);

  if (CASE == 1)
  {
      uint32_t nNodes = 2;
    double simTime = 100.0; // Simulation time in seconds

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Configure Point-to-Point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("60Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install NetDevices on nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install Video Stream Client on Node 1
    uint16_t port = 6969;
    VideoStreamClientHelper videoClient(interfaces.GetAddress(0), port);
    ApplicationContainer clientApp = videoClient.Install(nodes.Get(1));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(simTime));

    // Install Video Stream Server on Node 0
    VideoStreamServerHelper videoServer(port);
    videoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    videoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/frameList.txt"));
    ApplicationContainer serverApp = videoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set node positions for animation
    AnimationInterface anim("case_1.xml");
    anim.SetConstantPosition(nodes.Get(0), 1.0, 2.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 20.0);

    // Enable packet metadata for NetAnim
    anim.EnablePacketMetadata(true);

    // Enable pcap tracing (optional)
    pointToPoint.EnablePcap("videoStream_CASE_1", devices.Get(1), false);

    // --- FlowMonitor Integration Start ---
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();
    // --- FlowMonitor Integration End ---

    // Run the simulation
    Simulator::Stop(Seconds(simTime + 1.0)); // Ensure all applications have stopped
    Simulator::Run();

    // --- FlowMonitor Statistics Collection ---
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    // Open an output file to store metrics
    std::ofstream outFile("flowmon_metrics_CASE_1.csv", std::ios::out);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Could not open output file for writing metrics.");
        return 1;
    }

    // Write CSV header
    outFile << "FlowID,Source,Destination,TxPackets,RxPackets,Throughput(Mbps),Goodput(Mbps),AverageDelay(s),PacketLossRatio(%),PacketDeliveryRatio(%),AverageJitter(s),BandwidthUtilization(%),Retransmissions\n";

    // Print header to terminal
    std::cout << "FlowID\tSource\t\tDestination\tTxPackets\tRxPackets\tThroughput(Mbps)\tGoodput(Mbps)\tAverageDelay(s)\tPacketLossRatio(%)\tPacketDeliveryRatio(%)\tAverageJitter(s)\tBandwidthUtilization(%)\tRetransmissions\n";

    // Variables for Jain's Fairness Index
    double sumThroughput = 0.0;
    double sumSqThroughput = 0.0;
    uint32_t numFlows = 0;

    // Iterate through each flow and extract metrics
    for (auto &flow : stats)
    {
        FlowId flowId = flow.first;
        FlowMonitor::FlowStats flowStats = flow.second;

        // Get flow classifier information
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);

        // Calculate Time Duration
        double timeDuration = flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds();

        // Calculate Throughput (Mbps)
        double throughput = (flowStats.rxBytes * 8.0) / timeDuration / 1e6; // Mbps

        // Calculate Goodput (Mbps)
        // Assuming application data size is equal to rxBytes
        // If there is overhead, adjust accordingly
        double goodput = throughput; // In this case, assume no overhead

        // Calculate Average Delay (seconds)
        double averageDelay = 0.0;
        if (flowStats.rxPackets > 0)
        {
            averageDelay = flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
        }

        // Calculate Packet Loss Ratio (%)
        double packetLossRatio = 0.0;
        if (flowStats.txPackets > 0)
        {
            packetLossRatio = ((double)(flowStats.txPackets - flowStats.rxPackets) / flowStats.txPackets) * 100.0;
        }

        // Calculate Packet Delivery Ratio (%)
        double packetDeliveryRatio = 0.0;
        if (flowStats.txPackets > 0)
        {
            packetDeliveryRatio = ((double)flowStats.rxPackets / flowStats.txPackets) * 100.0;
        }

        // Calculate Average Jitter (seconds)
        double averageJitter = 0.0;
        if (flowStats.rxPackets > 1)
        {
            averageJitter = flowStats.jitterSum.GetSeconds() / (flowStats.rxPackets - 1);
        }

        // Calculate Bandwidth Utilization (%)
        double linkCapacity = 60.0; // Mbps
        double bandwidthUtilization = (throughput / linkCapacity) * 100.0;

        // Calculate Retransmissions
        // Assuming retransmissions are the difference between MAC-level transmissions and IP-level transmissions
        uint64_t retransmissions = 0;
        if (flowStats.txPackets > 0)
        {
            retransmissions = flowStats.txPackets - flowStats.rxPackets;
        }

        // Update variables for Jain's Fairness Index
        sumThroughput += throughput;
        sumSqThroughput += throughput * throughput;
        numFlows++;

        // Write metrics to CSV
        outFile << flowId << ","
                << t.sourceAddress << "," << t.destinationAddress << ","
                << flowStats.txPackets << "," << flowStats.rxPackets << ","
                << throughput << "," << goodput << ","
                << averageDelay << "," << packetLossRatio << ","
                << packetDeliveryRatio << "," << averageJitter << ","
                << bandwidthUtilization << "," << retransmissions << "\n";

        // Print metrics to terminal
        std::cout << flowId << "\t"
                  << t.sourceAddress << "\t"
                  << t.destinationAddress << "\t"
                  << flowStats.txPackets << "\t\t"
                  << flowStats.rxPackets << "\t\t"
                  << throughput << "\t\t"
                  << goodput << "\t\t"
                  << averageDelay << "\t\t"
                  << packetLossRatio << "\t\t"
                  << packetDeliveryRatio << "\t\t"
                  << averageJitter << "\t\t"
                  << bandwidthUtilization << "\t\t"
                  << retransmissions << "\n";
    }

    outFile.close();

    // Calculate Jain's Fairness Index
    double fairnessIndex = 0.0;
    if (numFlows > 0)
    {
        fairnessIndex = (sumThroughput * sumThroughput) / (numFlows * sumSqThroughput);
    }

    // Print Jain's Fairness Index
    std::cout << "\nJain's Fairness Index: " << fairnessIndex << std::endl;

    // --- FlowMonitor Statistics Collection End ---

    // Destroy the simulation
    Simulator::Destroy();

    // Notify user
    std::cout << "\nSimulation completed. Metrics have been saved to flowmon_metrics.csv.\n";f
  }

  else if (CASE == 2)
  {
        // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create node containers for point-to-point links
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));

    // Configure Point-to-Point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on nodes
    NetDeviceContainer d0d1 = pointToPoint.Install(n0n1);
    NetDeviceContainer d0d2 = pointToPoint.Install(n0n2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address1, address2;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address1.Assign(d0d1);

    address2.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address2.Assign(d0d2);

    // Packet sink applications for clients (optional if required)
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 80)));

    ApplicationContainer clientSinkApp1 = sinkHelper.Install(nodes.Get(1));
    clientSinkApp1.Start(Seconds(0.0));
    clientSinkApp1.Stop(Seconds(100.0));

    ApplicationContainer clientSinkApp2 = sinkHelper.Install(nodes.Get(2));
    clientSinkApp2.Start(Seconds(0.0));
    clientSinkApp2.Stop(Seconds(100.0));

    // Install Video Stream Clients
    uint16_t port = 6969;

    VideoStreamClientHelper videoClient1(i0i1.GetAddress(0), port);
    ApplicationContainer clientApp1 = videoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(100.0));

    VideoStreamClientHelper videoClient2(i0i2.GetAddress(0), port);
    ApplicationContainer clientApp2 = videoClient2.Install(nodes.Get(2));
    clientApp2.Start(Seconds(0.5));
    clientApp2.Stop(Seconds(100.0));

    // Install Video Stream Server on Node 0
    VideoStreamServerHelper videoServer(port);
    videoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    videoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/frameList.txt"));

    ApplicationContainer serverApp = videoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(100.0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // Set node positions for NetAnim
    AnimationInterface anim("case_2.xml");
    anim.SetConstantPosition(nodes.Get(0), 1.0, 2.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 30.0);
    anim.SetConstantPosition(nodes.Get(2), 40.0, 50.0);

    // Enable pcap tracing
    pointToPoint.EnablePcap("videoStream_d0d1_case2", d0d1.Get(1), false);
    pointToPoint.EnablePcap("videoStream_d0d2_case2", d0d2.Get(1), false);

    // Run the simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    // Flow Monitor Analysis
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    // Open an output file to store metrics
    std::ofstream outFile("flowmon_metrics_CASE_2.csv", std::ios::out);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Could not open output file for writing metrics.");
        return 1;
    }

    // Write CSV header
    outFile << "FlowID,Source,Destination,TxPackets,RxPackets,Throughput(Mbps),Goodput(Mbps),"
               "AverageDelay(s),PacketLossRatio(%),PacketDeliveryRatio(%),AverageJitter(s),"
               "BandwidthUtilization(%),Retransmissions\n";

    // Print header to terminal
    std::cout << "FlowID\tSource\t\tDestination\tTxPackets\tRxPackets\tThroughput(Mbps)\tGoodput(Mbps)\t"
                 "AverageDelay(s)\tPacketLossRatio(%)\tPacketDeliveryRatio(%)\tAverageJitter(s)\t"
                 "BandwidthUtilization(%)\tRetransmissions\n";

    // Variables for Jain's Fairness Index
    double sumThroughput = 0.0;
    double sumSqThroughput = 0.0;
    uint32_t numFlows = 0;

    // Process each flow
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator it = stats.begin(); it != stats.end(); ++it)
    {
        FlowId flowId = it->first;
        FlowMonitor::FlowStats flowStats = it->second;
        Ipv4FlowClassifier::FiveTuple flow = classifier->FindFlow(flowId);

        // Only consider flows from the server to clients
        if ((flow.sourceAddress == i0i1.GetAddress(0) && flow.destinationAddress == i0i1.GetAddress(1)) ||
            (flow.sourceAddress == i0i2.GetAddress(0) && flow.destinationAddress == i0i2.GetAddress(1)))
        {
            // Calculate metrics
            double timeDuration = flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds();
            double throughput = (flowStats.rxBytes * 8.0) / timeDuration / 1e6; // Mbps

            // Goodput (assuming no overhead for simplification)
            double goodput = throughput; // Adjust if overhead is known

            // Average Delay
            double averageDelay = 0.0;
            if (flowStats.rxPackets > 0)
            {
                averageDelay = flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
            }

            // Packet Loss Ratio
            double packetLossRatio = 0.0;
            if (flowStats.txPackets > 0)
            {
                packetLossRatio = ((double)(flowStats.txPackets - flowStats.rxPackets) / flowStats.txPackets) * 100.0;
            }

            // Packet Delivery Ratio
            double packetDeliveryRatio = 0.0;
            if (flowStats.txPackets > 0)
            {
                packetDeliveryRatio = ((double)flowStats.rxPackets / flowStats.txPackets) * 100.0;
            }

            // Average Jitter
            double averageJitter = 0.0;
            if (flowStats.rxPackets > 1)
            {
                averageJitter = flowStats.jitterSum.GetSeconds() / (flowStats.rxPackets - 1);
            }

            // Bandwidth Utilization
            double linkCapacity = 20.0; // Mbps
            double bandwidthUtilization = (throughput / linkCapacity) * 100.0;

            // Retransmissions (simplified estimation)
            uint64_t retransmissions = 0;
            if (flowStats.txPackets > flowStats.rxPackets)
            {
                retransmissions = flowStats.txPackets - flowStats.rxPackets;
            }

            // Update variables for Jain's Fairness Index
            sumThroughput += throughput;
            sumSqThroughput += throughput * throughput;
            numFlows++;

            // Write metrics to CSV
            outFile << flowId << ","
                    << flow.sourceAddress << "," << flow.destinationAddress << ","
                    << flowStats.txPackets << "," << flowStats.rxPackets << ","
                    << throughput << "," << goodput << ","
                    << averageDelay << "," << packetLossRatio << ","
                    << packetDeliveryRatio << "," << averageJitter << ","
                    << bandwidthUtilization << "," << retransmissions << "\n";

            // Print metrics to terminal
            std::cout << flowId << "\t"
                      << flow.sourceAddress << "\t"
                      << flow.destinationAddress << "\t"
                      << flowStats.txPackets << "\t\t"
                      << flowStats.rxPackets << "\t\t"
                      << throughput << "\t\t"
                      << goodput << "\t\t"
                      << averageDelay << "\t\t"
                      << packetLossRatio << "\t\t"
                      << packetDeliveryRatio << "\t\t"
                      << averageJitter << "\t\t"
                      << bandwidthUtilization << "\t\t"
                      << retransmissions << "\n";
        }
    }

    outFile.close();

    // Calculate Jain's Fairness Index
    double fairnessIndex = 0.0;
    if (numFlows > 0)
    {
        fairnessIndex = (sumThroughput * sumThroughput) / (numFlows * sumSqThroughput);
    }

    // Print Jain's Fairness Index
    std::cout << "\nJain's Fairness Index: " << fairnessIndex << std::endl;

    // Serialize flow monitor data to XML (optional)
    flowmon->SerializeToXmlFile("Case_2_flowmonitor.xml", true, true);

    // Destroy the simulation
    Simulator::Destroy();

    // Notify user
    std::cout << "\nSimulation completed. Metrics have been saved to flowmon_metrics_case2.csv.\n";
  }
  else if (CASE == 3)
  {
    const uint32_t nWifi = 3, nAp = 1;
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nWifi);  
    NodeContainer wifiApNode;
    wifiApNode.Create(nAp);   
    
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();   
    YansWifiPhyHelper phy;  
    phy.SetChannel (channel.Create ());  
  
    WifiHelper wifi;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");  
  
  
    WifiMacHelper mac; 
    Ssid ssid = Ssid ("ns-3-aqiao");  
    mac.SetType ("ns3::AdhocWifiMac",    
                "Ssid", SsidValue (ssid));  
  
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);  
  
    // mac.SetType ("ns3::ApWifiMac",   
    //             "Ssid", SsidValue (ssid));   
  
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);   

    MobilityHelper mobility; 
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (30.0),
                                 "DeltaY", DoubleValue (30.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
 
    
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));   
    mobility.Install (wifiStaNodes);
   
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");  
    mobility.Install (wifiApNode);
  
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);   
  
    Ipv4AddressHelper address;
  
    address.SetBase ("10.1.3.0", "255.255.255.0");
    
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign (apDevices); 
    Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces=address.Assign (staDevices);
                  
    //UdpEchoServerHelper echoServer (9);
    VideoStreamServerHelper videoServer (5000);
    videoServer.SetAttribute ("MaxPacketSize", UintegerValue (1400));
    videoServer.SetAttribute ("FrameFile", StringValue ("./scratch/videoStreamer/small.txt"));
    for(uint m=0; m<nAp; m++)
    {
      ApplicationContainer serverApps = videoServer.Install (wifiApNode.Get (m));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (100.0));
    }
  
    for(uint k=0; k<nWifi; k++)
    {
      VideoStreamClientHelper videoClient (apInterfaces.GetAddress (0), 5000);
      ApplicationContainer clientApps =
      videoClient.Install (wifiStaNodes.Get (k));
      clientApps.Start (Seconds (0.5));
      clientApps.Stop (Seconds (100.0));
    }
  
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
    Simulator::Stop (Seconds (10.0));
  
    phy.EnablePcap ("wifi-videoStream", apDevices.Get (0));
    AnimationInterface anim("wifi-1-3.xml");
    Simulator::Run ();
    Simulator::Destroy ();
  }
  else if (CASE == 4)
  {
    const uint32_t nWifi = 3, nAp = 3;
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nWifi);  
    NodeContainer wifiApNode;
    wifiApNode.Create(nAp);   
    
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();   
    YansWifiPhyHelper phy = YansWifiPhyHelper();  
    phy.SetChannel (channel.Create ());  
  
    WifiHelper wifi;
    

    wifi.SetRemoteStationManager ("ns3::IdealWifiManager");  
  
  
    WifiMacHelper mac; 
    Ssid ssid = Ssid ("ns-3-aqiao");  
    mac.SetType ("ns3::StaWifiMac",    
                "Ssid", SsidValue (ssid),   
                "ActiveProbing", BooleanValue (false));  
  
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);  
  
    mac.SetType ("ns3::ApWifiMac",   
                "Ssid", SsidValue (ssid));   
  
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);   
    MobilityHelper mobility; 
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (50.0),
                                  "DeltaY", DoubleValue (30.0),
                                  "GridWidth", UintegerValue (3),
                                  "LayoutType", StringValue ("RowFirst"));
  
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");  
    mobility.Install (wifiApNode);
      
    //mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",   
    //                           "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));   
    //mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");  
    mobility.Install (wifiStaNodes);
  
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);   
  
    Ipv4AddressHelper address;
  
    address.SetBase ("10.1.3.0", "255.255.255.0");
    
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign (apDevices); 
    Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces=address.Assign (staDevices);
                  
    //UdpEchoServerHelper echoServer (9);
    VideoStreamServerHelper videoServer (5000);
    videoServer.SetAttribute ("MaxPacketSize", UintegerValue (1400));
    videoServer.SetAttribute ("FrameFile", StringValue ("./scratch/videoStreamer/small.txt"));
    for(uint m=0; m<nAp; m++)
    {
      ApplicationContainer serverApps = videoServer.Install (wifiApNode.Get (m));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (100.0));
    }
  
    for(uint k=0; k<nWifi; k++)
    {
      VideoStreamClientHelper videoClient (apInterfaces.GetAddress (k), 5000);
      ApplicationContainer clientApps =
      videoClient.Install (wifiStaNodes.Get (k));
      clientApps.Start (Seconds (0.5));
      clientApps.Stop (Seconds (100.0));
    }
  
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
    Simulator::Stop (Seconds (10.0));
  
    phy.EnablePcap ("wifi-videoStream", apDevices.Get (0));
    AnimationInterface anim("wifi-1-3.xml");
    Simulator::Run ();
    Simulator::Destroy ();
  }
  else if(CASE==5){
    NodeContainer nodes;
nodes.Create (3);

PointToPointHelper pointToPoint;
pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

NetDeviceContainer devices;
devices.Add(pointToPoint.Install(nodes.Get(0), nodes.Get(1))); // Connect client 1 to server
devices.Add(pointToPoint.Install(nodes.Get(0), nodes.Get(2))); // Connect client 2 to server

InternetStackHelper stack;
stack.Install (nodes);

Ipv4AddressHelper address;
address.SetBase ("10.1.1.0", "255.255.255.0");

Ipv4InterfaceContainer interfaces = address.Assign (devices);

VideoStreamServerHelper videoServer (5000);
    videoServer.SetAttribute ("MaxPacketSize", UintegerValue (1400));
    videoServer.SetAttribute ("FrameFile", StringValue ("./scratch/videoStreamer/small.txt"));
    // videoServer.SetAttribute ("FrameSize", UintegerValue (4096));

    ApplicationContainer serverApp = videoServer.Install (nodes.Get (0));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (100.0));


    VideoStreamClientHelper videoClient1 (interfaces.GetAddress (0), 5000);
    ApplicationContainer clientApp1 = videoClient1.Install (nodes.Get (1));
    clientApp1.Start (Seconds (1.0));
    clientApp1.Stop (Seconds (100.0));

    VideoStreamClientHelper videoClient2 (interfaces.GetAddress (0), 5000);
    ApplicationContainer clientApp2 = videoClient2.Install (nodes.Get (2));
    clientApp2.Start (Seconds (0.5));
    clientApp2.Stop (Seconds (100.0));

 
    AnimationInterface anim("case_5.xml");
    anim.SetConstantPosition(nodes.Get(0),1.0,2.0);
    anim.SetConstantPosition(nodes.Get(1),10.0,20.0);
    anim.SetConstantPosition(nodes.Get(2),20.0,30.0);

    // pointToPoint.EnablePcap("videoStream", d0d1.Get(1), false);
    // pointToPoint.EnablePcap ("videoStream", d0d2.Get(1), false);
    Simulator::Run ();
    Simulator::Destroy ();





  }

  else if (CASE==6){
      // 1. Create nodes: Server, Router, and Client
    NodeContainer serverNode, routerNode, clientNode;
    serverNode.Create(1);
    routerNode.Create(1);
    clientNode.Create(1);

    // 2. Set up point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("60Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Server ↔ Router link
    NetDeviceContainer serverRouterDevices = pointToPoint.Install(serverNode.Get(0), routerNode.Get(0));

    // Router ↔ Client link
    NetDeviceContainer routerClientDevices = pointToPoint.Install(routerNode.Get(0), clientNode.Get(0));

    // 3. Install Internet stack
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(routerNode);
    stack.Install(clientNode);

    // 4. Assign IP addresses
    Ipv4AddressHelper address;

    // Server ↔ Router IPs
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverRouterInterfaces = address.Assign(serverRouterDevices);

    // Router ↔ Client IPs
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerClientInterfaces = address.Assign(routerClientDevices);

    // 5. Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Configure Mobility (positions for visualization)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(serverNode);
    mobility.Install(routerNode);
    mobility.Install(clientNode);

    serverNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    routerNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 0.0, 0.0));
    clientNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0));

    // 7. Set up Applications

    // Server Application (Server to Client)
    uint16_t port = 6969;
    VideoStreamServerHelper videoServer(port);
    videoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    videoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/frameList.txt"));

    ApplicationContainer serverApp = videoServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(100.0));

    // Client Application (Client to Server)
    Ipv4Address serverAddress = serverRouterInterfaces.GetAddress(0); // Server's IP
    VideoStreamClientHelper videoClient(serverAddress, port);

    ApplicationContainer clientApp = videoClient.Install(clientNode.Get(0));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(100.0));

    // Reverse Application (Client to Server)
    uint16_t reversePort = port + 1;
    VideoStreamServerHelper reverseVideoServer(reversePort);
    reverseVideoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    reverseVideoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/frameList.txt"));

    ApplicationContainer reverseServerApp = reverseVideoServer.Install(clientNode.Get(0));
    reverseServerApp.Start(Seconds(1.0));
    reverseServerApp.Stop(Seconds(100.0));

    VideoStreamClientHelper reverseVideoClient(routerClientInterfaces.GetAddress(1), reversePort);
    ApplicationContainer reverseClientApp = reverseVideoClient.Install(serverNode.Get(0));
    reverseClientApp.Start(Seconds(1.5));
    reverseClientApp.Stop(Seconds(100.0));

    // 8. NetAnim configuration
    AnimationInterface anim("video_stream_with_router_case_6.xml");

    // Node descriptions
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.UpdateNodeDescription(routerNode.Get(0), "Router");
    anim.UpdateNodeDescription(clientNode.Get(0), "Client");

    // Node colors
    anim.UpdateNodeColor(serverNode.Get(0), 0, 255, 0);    // Green
    anim.UpdateNodeColor(routerNode.Get(0), 255, 255, 0);  // Yellow
    anim.UpdateNodeColor(clientNode.Get(0), 0, 0, 255);    // Blue

    // 9. Install FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // 10. Run the simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    // 11. Flow Monitor Analysis
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    // Open an output file to store metrics
    std::ofstream outFile("flowmon_metrics_router_topology_case_6.csv", std::ios::out);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Could not open output file for writing metrics.");
        return 1;
    }

    // Write CSV header
    outFile << "FlowID,Source,Destination,TxPackets,RxPackets,Throughput(Mbps),Goodput(Mbps),"
               "AverageDelay(s),PacketLossRatio(%),PacketDeliveryRatio(%),AverageJitter(s),"
               "BandwidthUtilization(%),Retransmissions\n";

    // Print header to terminal
    std::cout << "FlowID\tSource\t\tDestination\tTxPackets\tRxPackets\tThroughput(Mbps)\tGoodput(Mbps)\t"
                 "AverageDelay(s)\tPacketLossRatio(%)\tPacketDeliveryRatio(%)\tAverageJitter(s)\t"
                 "BandwidthUtilization(%)\tRetransmissions\n";

    // Variables for Jain's Fairness Index
    double sumThroughput = 0.0;
    double sumSqThroughput = 0.0;
    uint32_t numFlows = 0;

    // Process each flow
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        FlowId flowId = it->first;
        FlowMonitor::FlowStats flowStats = it->second;
        Ipv4FlowClassifier::FiveTuple flow = classifier->FindFlow(flowId);

        // Process flows between server and client in both directions
        if ((flow.sourceAddress == serverRouterInterfaces.GetAddress(0) && flow.destinationAddress == routerClientInterfaces.GetAddress(1)) ||
            (flow.sourceAddress == routerClientInterfaces.GetAddress(1) && flow.destinationAddress == serverRouterInterfaces.GetAddress(0)))
        {
            // Calculate metrics
            double timeDuration = flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds();
            double throughput = (flowStats.rxBytes * 8.0) / timeDuration / 1e6; // Mbps

            // Goodput (assuming no overhead for simplification)
            double goodput = throughput; // Adjust if overhead is known

            // Average Delay
            double averageDelay = 0.0;
            if (flowStats.rxPackets > 0)
            {
                averageDelay = flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
            }

            // Packet Loss Ratio
            double packetLossRatio = 0.0;
            if (flowStats.txPackets > 0)
            {
                packetLossRatio = ((double)(flowStats.txPackets - flowStats.rxPackets) / flowStats.txPackets) * 100.0;
            }

            // Packet Delivery Ratio
            double packetDeliveryRatio = 0.0;
            if (flowStats.txPackets > 0)
            {
                packetDeliveryRatio = ((double)flowStats.rxPackets / flowStats.txPackets) * 100.0;
            }

            // Average Jitter
            double averageJitter = 0.0;
            if (flowStats.rxPackets > 1)
            {
                averageJitter = flowStats.jitterSum.GetSeconds() / (flowStats.rxPackets - 1);
            }

            // Bandwidth Utilization
            double linkCapacity = 60.0; // Mbps
            double bandwidthUtilization = (throughput / linkCapacity) * 100.0;

            // Retransmissions (simplified estimation)
            uint64_t retransmissions = 0;
            if (flowStats.txPackets > flowStats.rxPackets)
            {
                retransmissions = flowStats.txPackets - flowStats.rxPackets;
            }

            // Update variables for Jain's Fairness Index
            sumThroughput += throughput;
            sumSqThroughput += throughput * throughput;
            numFlows++;

            // Write metrics to CSV
            outFile << flowId << ","
                    << flow.sourceAddress << "," << flow.destinationAddress << ","
                    << flowStats.txPackets << "," << flowStats.rxPackets << ","
                    << throughput << "," << goodput << ","
                    << averageDelay << "," << packetLossRatio << ","
                    << packetDeliveryRatio << "," << averageJitter << ","
                    << bandwidthUtilization << "," << retransmissions << "\n";

            // Print metrics to terminal
            std::cout << flowId << "\t"
                      << flow.sourceAddress << "\t"
                      << flow.destinationAddress << "\t"
                      << flowStats.txPackets << "\t\t"
                      << flowStats.rxPackets << "\t\t"
                      << throughput << "\t\t"
                      << goodput << "\t\t"
                      << averageDelay << "\t\t"
                      << packetLossRatio << "\t\t"
                      << packetDeliveryRatio << "\t\t"
                      << averageJitter << "\t\t"
                      << bandwidthUtilization << "\t\t"
                      << retransmissions << "\n";
        }
    }

    outFile.close();

    // Calculate Jain's Fairness Index
    double fairnessIndex = 0.0;
    if (numFlows > 0)
    {
        fairnessIndex = (sumThroughput * sumThroughput) / (numFlows * sumSqThroughput);
    }

    // Print Jain's Fairness Index
    std::cout << "\nJain's Fairness Index: " << fairnessIndex << std::endl;

    // Serialize flow monitor data to XML (optional)
    flowmon->SerializeToXmlFile("router_topology_flowmonitor_case_6.xml", true, true);

    // 12. Destroy the simulation
    Simulator::Destroy();

    // Notify user
    std::cout << "\nSimulation completed. Metrics have been saved to flowmon_metrics_router_topology.csv.\n";
  }
  else if (CASE==7){
  // 1. Create nodes: Server, Router, Client 1, and Client 2
    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer routerNode;
    routerNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(2); // Client 1 and Client 2

    // 2. Set up point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("60Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Server ↔ Router link
    NetDeviceContainer serverRouterDevices = pointToPoint.Install(serverNode.Get(0), routerNode.Get(0));

    // Router ↔ Client 1 link
    NetDeviceContainer routerClient1Devices = pointToPoint.Install(routerNode.Get(0), clientNodes.Get(0));

    // Router ↔ Client 2 link
    NetDeviceContainer routerClient2Devices = pointToPoint.Install(routerNode.Get(0), clientNodes.Get(1));

    // 3. Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(routerNode);
    stack.Install(clientNodes);

    // 4. Assign IP addresses
    Ipv4AddressHelper address;

    // Server ↔ Router IPs
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverRouterInterfaces = address.Assign(serverRouterDevices);

    // Router ↔ Client 1 IPs
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerClient1Interfaces = address.Assign(routerClient1Devices);

    // Router ↔ Client 2 IPs
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer routerClient2Interfaces = address.Assign(routerClient2Devices);

    // 5. Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Configure Mobility (positions for visualization)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Install mobility on nodes
    mobility.Install(serverNode);
    mobility.Install(routerNode);
    mobility.Install(clientNodes);

    // Set positions
    serverNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));     // Server
    routerNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 0.0, 0.0));     // Router
    clientNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(10.0, -2.0, 0.0));  // Client 1
    clientNodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(10.0, 2.0, 0.0));   // Client 2

    // 7. Set up Applications

    // Ports for applications
    uint16_t portSC1 = 7000; // Server ↔ Client 1
    uint16_t portSC2 = 7001; // Server ↔ Client 2
    uint16_t portC1C2 = 7002; // Client 1 ↔ Client 2

    // --- Applications between Server and Client 1 ---

    // Server to Client 1
    VideoStreamServerHelper serverAppSC1(portSC1);
    serverAppSC1.SetAttribute("MaxPacketSize", UintegerValue(1400));
    serverAppSC1.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));
    ApplicationContainer serverAppSC1Container = serverAppSC1.Install(serverNode.Get(0));
    serverAppSC1Container.Start(Seconds(0.0));
    serverAppSC1Container.Stop(Seconds(100.0));

    VideoStreamClientHelper clientAppSC1(serverRouterInterfaces.GetAddress(0), portSC1);
    ApplicationContainer clientAppSC1Container = clientAppSC1.Install(clientNodes.Get(0));
    clientAppSC1Container.Start(Seconds(0.5));
    clientAppSC1Container.Stop(Seconds(100.0));

    // Client 1 to Server
    VideoStreamServerHelper serverAppC1S(portSC1 + 10);
    serverAppC1S.SetAttribute("MaxPacketSize", UintegerValue(1400));
    serverAppC1S.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));
    ApplicationContainer serverAppC1SContainer = serverAppC1S.Install(clientNodes.Get(0));
    serverAppC1SContainer.Start(Seconds(1.0));
    serverAppC1SContainer.Stop(Seconds(100.0));

    VideoStreamClientHelper clientAppC1S(routerClient1Interfaces.GetAddress(1), portSC1 + 10);
    ApplicationContainer clientAppC1SContainer = clientAppC1S.Install(serverNode.Get(0));
    clientAppC1SContainer.Start(Seconds(1.5));
    clientAppC1SContainer.Stop(Seconds(100.0));

    // --- Applications between Server and Client 2 ---

    // Server to Client 2
    VideoStreamServerHelper serverAppSC2(portSC2);
    serverAppSC2.SetAttribute("MaxPacketSize", UintegerValue(1400));
    serverAppSC2.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));
    ApplicationContainer serverAppSC2Container = serverAppSC2.Install(serverNode.Get(0));
    serverAppSC2Container.Start(Seconds(0.0));
    serverAppSC2Container.Stop(Seconds(100.0));

    VideoStreamClientHelper clientAppSC2(serverRouterInterfaces.GetAddress(0), portSC2);
    ApplicationContainer clientAppSC2Container = clientAppSC2.Install(clientNodes.Get(1));
    clientAppSC2Container.Start(Seconds(0.5));
    clientAppSC2Container.Stop(Seconds(100.0));

    // Client 2 to Server
    VideoStreamServerHelper serverAppC2S(portSC2 + 10);
    serverAppC2S.SetAttribute("MaxPacketSize", UintegerValue(1400));
    serverAppC2S.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));
    ApplicationContainer serverAppC2SContainer = serverAppC2S.Install(clientNodes.Get(1));
    serverAppC2SContainer.Start(Seconds(1.0));
    serverAppC2SContainer.Stop(Seconds(100.0));

    VideoStreamClientHelper clientAppC2S(routerClient2Interfaces.GetAddress(1), portSC2 + 10);
    ApplicationContainer clientAppC2SContainer = clientAppC2S.Install(serverNode.Get(0));
    clientAppC2SContainer.Start(Seconds(1.5));
    clientAppC2SContainer.Stop(Seconds(100.0));

    // --- Applications between Client 1 and Client 2 ---

    // Client 1 to Client 2
    VideoStreamServerHelper serverAppC1C2(portC1C2);
    serverAppC1C2.SetAttribute("MaxPacketSize", UintegerValue(1400));
    serverAppC1C2.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));
    ApplicationContainer serverAppC1C2Container = serverAppC1C2.Install(clientNodes.Get(0));
    serverAppC1C2Container.Start(Seconds(2.0));
    serverAppC1C2Container.Stop(Seconds(100.0));

    VideoStreamClientHelper clientAppC1C2(routerClient1Interfaces.GetAddress(1), portC1C2);
    ApplicationContainer clientAppC1C2Container = clientAppC1C2.Install(clientNodes.Get(1));
    clientAppC1C2Container.Start(Seconds(2.5));
    clientAppC1C2Container.Stop(Seconds(100.0));

    // Client 2 to Client 1
    VideoStreamServerHelper serverAppC2C1(portC1C2 + 10);
    serverAppC2C1.SetAttribute("MaxPacketSize", UintegerValue(1400));
    serverAppC2C1.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));
    ApplicationContainer serverAppC2C1Container = serverAppC2C1.Install(clientNodes.Get(1));
    serverAppC2C1Container.Start(Seconds(3.0));
    serverAppC2C1Container.Stop(Seconds(100.0));

    VideoStreamClientHelper clientAppC2C1(routerClient2Interfaces.GetAddress(1), portC1C2 + 10);
    ApplicationContainer clientAppC2C1Container = clientAppC2C1.Install(clientNodes.Get(0));
    clientAppC2C1Container.Start(Seconds(3.5));
    clientAppC2C1Container.Stop(Seconds(100.0));

    // 8. NetAnim configuration
    AnimationInterface anim("video_stream_with_router_two_clients_all_paths_case_7.xml");

    // Node descriptions
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.UpdateNodeDescription(routerNode.Get(0), "Router");
    anim.UpdateNodeDescription(clientNodes.Get(0), "Client1");
    anim.UpdateNodeDescription(clientNodes.Get(1), "Client2");

    // Node colors
    anim.UpdateNodeColor(serverNode.Get(0), 0, 255, 0);         // Green
    anim.UpdateNodeColor(routerNode.Get(0), 255, 255, 0);       // Yellow
    anim.UpdateNodeColor(clientNodes.Get(0), 0, 0, 255);        // Blue (Client 1)
    anim.UpdateNodeColor(clientNodes.Get(1), 255, 0, 0);        // Red (Client 2)

    // 9. Install FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    // 10. Run the simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();

    // 11. Flow Monitor Analysis
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    // Open an output file to store metrics
    std::ofstream outFile("flowmon_metrics_all_paths_case_7.csv", std::ios::out);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Could not open output file for writing metrics.");
        return 1;
    }

    // Write CSV header
    outFile << "FlowID,Source,Destination,TxPackets,RxPackets,Throughput(Mbps),Goodput(Mbps),"
               "AverageDelay(s),PacketLossRatio(%),PacketDeliveryRatio(%),AverageJitter(s),"
               "BandwidthUtilization(%),Retransmissions\n";

    // Print header to terminal
    std::cout << "FlowID\tSource\t\tDestination\tTxPackets\tRxPackets\tThroughput(Mbps)\tGoodput(Mbps)\t"
                 "AverageDelay(s)\tPacketLossRatio(%)\tPacketDeliveryRatio(%)\tAverageJitter(s)\t"
                 "BandwidthUtilization(%)\tRetransmissions\n";

    // Variables for Jain's Fairness Index
    double sumThroughput = 0.0;
    double sumSqThroughput = 0.0;
    uint32_t numFlows = 0;

    // Process each flow
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        FlowId flowId = it->first;
        FlowMonitor::FlowStats flowStats = it->second;
        Ipv4FlowClassifier::FiveTuple flow = classifier->FindFlow(flowId);

        // Process flows between all nodes
        // Identify flows based on source and destination addresses and ports
        bool relevantFlow = false;

        // Server ↔ Client 1
        if ((flow.sourceAddress == serverRouterInterfaces.GetAddress(0) && flow.destinationAddress == routerClient1Interfaces.GetAddress(1) && flow.sourcePort == portSC1) ||
            (flow.sourceAddress == routerClient1Interfaces.GetAddress(1) && flow.destinationAddress == serverRouterInterfaces.GetAddress(0) && flow.destinationPort == portSC1 + 10))
        {
            relevantFlow = true;
        }
        // Server ↔ Client 2
        else if ((flow.sourceAddress == serverRouterInterfaces.GetAddress(0) && flow.destinationAddress == routerClient2Interfaces.GetAddress(1) && flow.sourcePort == portSC2) ||
                 (flow.sourceAddress == routerClient2Interfaces.GetAddress(1) && flow.destinationAddress == serverRouterInterfaces.GetAddress(0) && flow.destinationPort == portSC2 + 10))
        {
            relevantFlow = true;
        }
        // Client 1 ↔ Client 2
        else if ((flow.sourceAddress == routerClient1Interfaces.GetAddress(1) && flow.destinationAddress == routerClient2Interfaces.GetAddress(1) && flow.sourcePort == portC1C2) ||
                 (flow.sourceAddress == routerClient2Interfaces.GetAddress(1) && flow.destinationAddress == routerClient1Interfaces.GetAddress(1) && flow.sourcePort == portC1C2 + 10))
        {
            relevantFlow = true;
        }

        if (relevantFlow)
        {
            // Calculate metrics
            double timeDuration = flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds();
            double throughput = (flowStats.rxBytes * 8.0) / timeDuration / 1e6; // Mbps

            // Goodput (assuming no overhead for simplification)
            double goodput = throughput; // Adjust if overhead is known

            // Average Delay
            double averageDelay = 0.0;
            if (flowStats.rxPackets > 0)
            {
                averageDelay = flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
            }

            // Packet Loss Ratio
            double packetLossRatio = 0.0;
            if (flowStats.txPackets > 0)
            {
                packetLossRatio = ((double)(flowStats.txPackets - flowStats.rxPackets) / flowStats.txPackets) * 100.0;
            }

            // Packet Delivery Ratio
            double packetDeliveryRatio = 0.0;
            if (flowStats.txPackets > 0)
            {
                packetDeliveryRatio = ((double)flowStats.rxPackets / flowStats.txPackets) * 100.0;
            }

            // Average Jitter
            double averageJitter = 0.0;
            if (flowStats.rxPackets > 1)
            {
                averageJitter = flowStats.jitterSum.GetSeconds() / (flowStats.rxPackets - 1);
            }

            // Bandwidth Utilization
            double linkCapacity = 60.0; // Mbps
            double bandwidthUtilization = (throughput / linkCapacity) * 100.0;

            // Retransmissions (simplified estimation)
            uint64_t retransmissions = 0;
            if (flowStats.txPackets > flowStats.rxPackets)
            {
                retransmissions = flowStats.txPackets - flowStats.rxPackets;
            }

            // Update variables for Jain's Fairness Index
            sumThroughput += throughput;
            sumSqThroughput += throughput * throughput;
            numFlows++;

            // Write metrics to CSV
            outFile << flowId << ","
                    << flow.sourceAddress << "," << flow.destinationAddress << ","
                    << flowStats.txPackets << "," << flowStats.rxPackets << ","
                    << throughput << "," << goodput << ","
                    << averageDelay << "," << packetLossRatio << ","
                    << packetDeliveryRatio << "," << averageJitter << ","
                    << bandwidthUtilization << "," << retransmissions << "\n";

            // Print metrics to terminal
            std::cout << flowId << "\t"
                      << flow.sourceAddress << "\t"
                      << flow.destinationAddress << "\t"
                      << flowStats.txPackets << "\t\t"
                      << flowStats.rxPackets << "\t\t"
                      << throughput << "\t\t"
                      << goodput << "\t\t"
                      << averageDelay << "\t\t"
                      << packetLossRatio << "\t\t"
                      << packetDeliveryRatio << "\t\t"
                      << averageJitter << "\t\t"
                      << bandwidthUtilization << "\t\t"
                      << retransmissions << "\n";
        }
    }

    outFile.close();

    // Calculate Jain's Fairness Index
    double fairnessIndex = 0.0;
    if (numFlows > 0)
    {
        fairnessIndex = (sumThroughput * sumThroughput) / (numFlows * sumSqThroughput);
    }

    // Print Jain's Fairness Index
    std::cout << "\nJain's Fairness Index: " << fairnessIndex << std::endl;

    // Serialize flow monitor data to XML (optional)
    flowmon->SerializeToXmlFile("all_paths_flowmonitor_case_7.xml", true, true);

    // 12. Destroy the simulation
    Simulator::Destroy();

    // Notify user
    std::cout << "\nSimulation completed. Metrics have been saved to flowmon_metrics_all_paths.csv.\n";
  }
  else if (CASE==8){
    // 1. Create nodes
    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer routerNode;
    routerNode.Create(1);

    NodeContainer bottleneckNode;
    bottleneckNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(2); // You can adjust the number of clients as needed

    // 2. Set up point-to-point links

    // Server ↔ Router link (High bandwidth)
    PointToPointHelper p2pHighBW;
    p2pHighBW.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pHighBW.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer serverRouterDevices = p2pHighBW.Install(serverNode.Get(0), routerNode.Get(0));

    // Router ↔ Bottleneck Node link (Limited bandwidth - bottleneck)
    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps")); // Bottleneck bandwidth
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer routerBottleneckDevices = p2pBottleneck.Install(routerNode.Get(0), bottleneckNode.Get(0));

    // Bottleneck Node ↔ Clients links (High bandwidth)
    NetDeviceContainer bottleneckClientDevices;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        NetDeviceContainer tempDevices = p2pHighBW.Install(bottleneckNode.Get(0), clientNodes.Get(i));
        bottleneckClientDevices.Add(tempDevices);
    }

    // 3. Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(routerNode);
    stack.Install(bottleneckNode);
    stack.Install(clientNodes);

    // 4. Assign IP addresses

    // Server ↔ Router
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverRouterInterfaces = address.Assign(serverRouterDevices);

    // Router ↔ Bottleneck Node
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerBottleneckInterfaces = address.Assign(routerBottleneckDevices);

    // Bottleneck Node ↔ Clients
    std::vector<Ipv4InterfaceContainer> bottleneckClientInterfaces;
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i + 3) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interface = address.Assign(NetDeviceContainer(bottleneckClientDevices.Get(i * 2), bottleneckClientDevices.Get(i * 2 + 1)));
        bottleneckClientInterfaces.push_back(interface);
    }

    // 5. Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Configure Mobility (for visualization)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(serverNode);
    mobility.Install(routerNode);
    mobility.Install(bottleneckNode);
    mobility.Install(clientNodes);

    // Set positions
    serverNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    routerNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0));
    bottleneckNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(20.0, 0.0, 0.0));

    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        clientNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(Vector(30.0, i * 5.0 - 5.0, 0.0));
    }

    // 7. Set up Applications

    // Server Application
    uint16_t port = 6969;
    VideoStreamServerHelper videoServer(port);
    videoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    videoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/frameList.txt"));

    ApplicationContainer serverApp = videoServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(100.0));

    // Client Applications
    ApplicationContainer clientApps;

    Ipv4Address serverAddress = serverRouterInterfaces.GetAddress(0); // Server's IP

    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        VideoStreamClientHelper videoClient(serverAddress, port);
        ApplicationContainer clientApp = videoClient.Install(clientNodes.Get(i));
        clientApp.Start(Seconds(1.0)); // Stagger client start times if desired
        clientApp.Stop(Seconds(100.0));
        clientApps.Add(clientApp);
    }

    // 8. NetAnim configuration
    AnimationInterface anim("video_stream_bottleneck.xml");

    // Node descriptions
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.UpdateNodeDescription(routerNode.Get(0), "Router");
    anim.UpdateNodeDescription(bottleneckNode.Get(0), "BottleneckNode");

    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        std::ostringstream nodeName;
        nodeName << "Client" << i + 1;
        anim.UpdateNodeDescription(clientNodes.Get(i), nodeName.str().c_str());
    }

    // Node colors
    anim.UpdateNodeColor(serverNode.Get(0), 0, 255, 0);         // Green
    anim.UpdateNodeColor(routerNode.Get(0), 255, 255, 0);       // Yellow
    anim.UpdateNodeColor(bottleneckNode.Get(0), 255, 165, 0);   // Orange

    for (uint32_t i = 0; i < clientNodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(clientNodes.Get(i), 0, 0, 255); // Blue
    }

    // 9. Run the simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();
  }
  else if (CASE==9){
      // 1. Create nodes

    // Server node
    Ptr<Node> serverNode = CreateObject<Node>();

    // Core Router
    Ptr<Node> coreRouter = CreateObject<Node>();

    // Aggregation Routers
    NodeContainer aggRouters;
    aggRouters.Create(2); // AggRouter1 and AggRouter2

    // Edge Routers
    NodeContainer edgeRouters;
    edgeRouters.Create(4); // EdgeR1, EdgeR2, EdgeR3, EdgeR4

    // Clients
    NodeContainer clients;
    clients.Create(4); // Client1, Client2, Client3, Client4

    // 2. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(coreRouter);
    stack.Install(aggRouters);
    stack.Install(edgeRouters);
    stack.Install(clients);

    // 3. Set up point-to-point links

    // Helper for high bandwidth links
    PointToPointHelper p2pHighBW;
    p2pHighBW.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pHighBW.SetChannelAttribute("Delay", StringValue("2ms"));

    // Helper for medium bandwidth links
    PointToPointHelper p2pMedBW;
    p2pMedBW.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    p2pMedBW.SetChannelAttribute("Delay", StringValue("5ms"));

    // Helper for low bandwidth links
    PointToPointHelper p2pLowBW;
    p2pLowBW.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLowBW.SetChannelAttribute("Delay", StringValue("10ms"));

    // Server ↔ Core Router
    NetDeviceContainer serverCoreDevices = p2pHighBW.Install(serverNode, coreRouter);

    // Core Router ↔ Aggregation Routers
    NetDeviceContainer coreAggDevices[2];
    coreAggDevices[0] = p2pHighBW.Install(coreRouter, aggRouters.Get(0)); // Core ↔ AggRouter1
    coreAggDevices[1] = p2pHighBW.Install(coreRouter, aggRouters.Get(1)); // Core ↔ AggRouter2

    // Aggregation Routers ↔ Edge Routers
    NetDeviceContainer aggEdgeDevices[4];
    aggEdgeDevices[0] = p2pMedBW.Install(aggRouters.Get(0), edgeRouters.Get(0)); // AggRouter1 ↔ EdgeR1
    aggEdgeDevices[1] = p2pMedBW.Install(aggRouters.Get(0), edgeRouters.Get(1)); // AggRouter1 ↔ EdgeR2
    aggEdgeDevices[2] = p2pMedBW.Install(aggRouters.Get(1), edgeRouters.Get(2)); // AggRouter2 ↔ EdgeR3
    aggEdgeDevices[3] = p2pMedBW.Install(aggRouters.Get(1), edgeRouters.Get(3)); // AggRouter2 ↔ EdgeR4

    // Edge Routers ↔ Clients
    NetDeviceContainer edgeClientDevices[4];
    for (uint32_t i = 0; i < clients.GetN(); ++i)
    {
        edgeClientDevices[i] = p2pLowBW.Install(edgeRouters.Get(i), clients.Get(i)); // EdgeRx ↔ Clientx
    }

    // 4. Assign IP addresses
    Ipv4AddressHelper address;

    // Server ↔ Core Router
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverCoreInterfaces = address.Assign(serverCoreDevices);

    // Core Router ↔ Aggregation Routers
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer coreAgg1Interfaces = address.Assign(coreAggDevices[0]);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer coreAgg2Interfaces = address.Assign(coreAggDevices[1]);

    // Aggregation Routers ↔ Edge Routers
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (4 + i) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(aggEdgeDevices[i]);
    }

    // Edge Routers ↔ Clients
    for (uint32_t i = 0; i < clients.GetN(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (8 + i) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        address.Assign(edgeClientDevices[i]);
    }

    // 5. Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Configure Mobility (for visualization)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Install mobility on all nodes
    mobility.Install(serverNode);
    mobility.Install(coreRouter);
    mobility.Install(aggRouters);
    mobility.Install(edgeRouters);
    mobility.Install(clients);

    // Set positions (for NetAnim visualization)
    serverNode->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    coreRouter->GetObject<MobilityModel>()->SetPosition(Vector(10.0, 0.0, 0.0));

    aggRouters.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(20.0, -10.0, 0.0)); // AggRouter1
    aggRouters.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(20.0, 10.0, 0.0));  // AggRouter2

    edgeRouters.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(30.0, -15.0, 0.0)); // EdgeR1
    edgeRouters.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(30.0, -5.0, 0.0));  // EdgeR2
    edgeRouters.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(30.0, 5.0, 0.0));   // EdgeR3
    edgeRouters.Get(3)->GetObject<MobilityModel>()->SetPosition(Vector(30.0, 15.0, 0.0));  // EdgeR4

    clients.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(40.0, -15.0, 0.0)); // Client1
    clients.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(40.0, -5.0, 0.0));  // Client2
    clients.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(40.0, 5.0, 0.0));   // Client3
    clients.Get(3)->GetObject<MobilityModel>()->SetPosition(Vector(40.0, 15.0, 0.0));  // Client4

    // 7. Set up Applications

    // Server Application
    uint16_t port = 6969;
    VideoStreamServerHelper videoServer(port);
    videoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    videoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/frameList.txt"));

    ApplicationContainer serverApp = videoServer.Install(serverNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(100.0));

    // Client Applications
    ApplicationContainer clientApps;

    Ipv4Address serverAddress = serverCoreInterfaces.GetAddress(0); // Server's IP

    for (uint32_t i = 0; i < clients.GetN(); ++i)
    {
        VideoStreamClientHelper videoClient(serverAddress, port);
        ApplicationContainer clientApp = videoClient.Install(clients.Get(i));
        clientApp.Start(Seconds(1.0)); // Stagger client start times if desired
        clientApp.Stop(Seconds(100.0));
        clientApps.Add(clientApp);
    }

    // 8. NetAnim configuration
    AnimationInterface anim("video_stream_hierarchical.xml");

    // Node descriptions
    anim.UpdateNodeDescription(serverNode, "Server");
    anim.UpdateNodeDescription(coreRouter, "CoreRouter");

    anim.UpdateNodeDescription(aggRouters.Get(0), "AggRouter1");
    anim.UpdateNodeDescription(aggRouters.Get(1), "AggRouter2");

    anim.UpdateNodeDescription(edgeRouters.Get(0), "EdgeR1");
    anim.UpdateNodeDescription(edgeRouters.Get(1), "EdgeR2");
    anim.UpdateNodeDescription(edgeRouters.Get(2), "EdgeR3");
    anim.UpdateNodeDescription(edgeRouters.Get(3), "EdgeR4");

    for (uint32_t i = 0; i < clients.GetN(); ++i)
    {
        std::ostringstream nodeName;
        nodeName << "Client" << i + 1;
        anim.UpdateNodeDescription(clients.Get(i), nodeName.str().c_str());
    }

    // Node colors
    anim.UpdateNodeColor(serverNode, 0, 255, 0);           // Green
    anim.UpdateNodeColor(coreRouter, 255, 255, 0);         // Yellow

    anim.UpdateNodeColor(aggRouters.Get(0), 255, 165, 0);  // Orange
    anim.UpdateNodeColor(aggRouters.Get(1), 255, 165, 0);  // Orange

    anim.UpdateNodeColor(edgeRouters.Get(0), 255, 192, 203); // Pink
    anim.UpdateNodeColor(edgeRouters.Get(1), 255, 192, 203); // Pink
    anim.UpdateNodeColor(edgeRouters.Get(2), 255, 192, 203); // Pink
    anim.UpdateNodeColor(edgeRouters.Get(3), 255, 192, 203); // Pink

    for (uint32_t i = 0; i < clients.GetN(); ++i)
    {
        anim.UpdateNodeColor(clients.Get(i), 0, 0, 255); // Blue
    }

    // 9. Run the simulation
    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();
  }
  else if(CASE==10){
     uint32_t nWifi = 3; // Number of STAs
    uint32_t nAp = 1;   // Number of APs

    // Enable logging for the applications
    LogComponentEnable("VideoStreamServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("VideoStreamClientApplication", LOG_LEVEL_INFO);

    // 1. Create Nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);

    NodeContainer wifiApNode;
    wifiApNode.Create(nAp);

    // 2. Configure Wi-Fi Channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // 3. Configure Wi-Fi MAC and Helper
    WifiHelper wifi;
   
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    Ssid ssid = Ssid("ns-3-ssid");

    // Configure STA devices
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // // Configure AP device
    // mac.SetType("ns3::ApWifiMac",
    //             "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // 4. Mobility Configuration
    MobilityHelper mobility;

    // Set position allocator for STAs
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    // Set mobility model for STAs
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));

    mobility.Install(wifiStaNodes);

    // Set mobility model for AP
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // 5. Install Internet Stack and Assign IP Addresses
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // 6. Install Applications

    // Server Application (on AP node)
    uint16_t port = 5000;
    VideoStreamServerHelper videoServer(port);
    videoServer.SetAttribute("MaxPacketSize", UintegerValue(1400));
    videoServer.SetAttribute("FrameFile", StringValue("./scratch/videoStreamer/small.txt"));

    ApplicationContainer serverApps = videoServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(100.0));

    // Client Applications
    for (uint32_t i = 0; i < nWifi; ++i)
    {
        VideoStreamClientHelper videoClient(apInterface.GetAddress(0), port);
        ApplicationContainer clientApp = videoClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i)); // Stagger start times if desired
        clientApp.Stop(Seconds(100.0));
    }

    // 7. Enable Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 8. NetAnim configuration
    AnimationInterface anim("wifi_topo_10.xml");

    // Enable packet metadata for NetAnim to display packet flows
    anim.EnablePacketMetadata(true);

    // Node descriptions
    anim.UpdateNodeDescription(wifiApNode.Get(0), "AP/Server");
    for (uint32_t i = 0; i < nWifi; ++i)
    {
        std::ostringstream nodeName;
        nodeName << "STA/Client" << i + 1;
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), nodeName.str().c_str());
    }

    // Node colors
    anim.UpdateNodeColor(wifiApNode.Get(0), 0, 255, 0); // Green
    for (uint32_t i = 0; i < nWifi; ++i)
    {
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 0, 0, 255); // Blue
    }

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();
  }
  return 0;
}