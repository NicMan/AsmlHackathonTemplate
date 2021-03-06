//
//! \file
//
// Created by Sander van Woensel / Umut Uyumaz
// Copyright (c) 2018 ASML Netherlands B.V. All rights reserved.
//
//! Mesh Network wrapper class to provide a container to add specific
//! mesh network behaviour.

#include "Facilities_MeshNetwork.hpp"

#include "Debug.hpp"
#include "painlessMesh.h"
#include <cassert>

#ifdef ESP8266
#include "Hash.h"
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

uint32_t to_int(String& msg) {
  uint32_t ans = 0;
  for(unsigned i=0; i<msg.length(); i++) {
    ans = ans*10 + (msg[i]-'0');
  }
  return ans;
}
String to_string(uint32_t n) {
  if(n == 0) {
    return "";
  }
  String ans = "";
  while(n > 0) {
	  ans = ('0' + (n%10)) + ans;
  }
	return ans;
}
bool has_prefix(String& msg, String& prefix) {
  if(msg.length() < prefix.length()) { return false; }
  for(unsigned i=0; i<prefix.length(); i++) {
    if(msg[i] != prefix[i]) {
      return false;
    }
  }
  return true;
}
String remove_prefix(String& msg, String& prefix) {
  assert(has_prefix(msg, prefix));
  String ans = "";
  for(unsigned i=prefix.length(); i<msg.length(); i++) {
    ans += msg[i];
  }
  return ans;
}



namespace Facilities {

const uint16_t MeshNetwork::PORT = 5555;
AsyncWebServer server(80);

//! Construct only.
//! \note Does not construct and initialize in one go to be able to initialize after serial debug port has been opened.
MeshNetwork::MeshNetwork()
{
   m_mesh.onReceive(std::bind(&MeshNetwork::receivedCb, this, std::placeholders::_1, std::placeholders::_2));
   //m_mesh.onNewConnection(...);
   //m_mesh.onChangedConnections(...);
   //m_mesh.onNodeTimeAdjusted(...);
}

// Initialize mesh network.
void MeshNetwork::initialize(const __FlashStringHelper *prefix, const __FlashStringHelper *password, Scheduler& taskScheduler)
{
   // Set debug messages before init() so that you can see startup messages.
   m_mesh.setDebugMsgTypes( ERROR | STARTUP );  // To enable all: ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE
   m_mesh.init( prefix, password, &taskScheduler, MeshNetwork::PORT );
   
   server.on("/", HTTP_GET, [&](AsyncWebServerRequest *request){
	   String msg = "";
       if (request->hasArg("BROADCAST")){
         msg = request->arg("BROADCAST");
         m_mesh.sendBroadcast(msg, true);
       }
       request->send(200, "text/html", "<form>Text to Broadcast<br><input type='text' name='BROADCAST' value='" + msg + "'><br><br><input type='submit' value='Submit'></form>");
   });
   server.on("/debug", HTTP_GET, [&](AsyncWebServerRequest *request){
     request->send(200, "text/html", "Frame Rate: 100hz\nMissed Frames: 0\nAnimation time: 10s\nNodes:" + to_string(m_nodes.size()) + "\nHistory:\n" + m_history);
   });

   //Amount of nodes actively functioning.
   //History record of removed and added nodes since the system became active (i.e. running with at least one node).
   
   server.begin();
}

//! Update mesh; forward call to painlessMesh.
void MeshNetwork::update()
{
   m_mesh.update();
}

void MeshNetwork::sendBroadcast(String &message, bool include_self)
{
   MY_DEBUG_PRINT("Broadcasting message: "); MY_DEBUG_PRINTLN(message);
   m_mesh.sendBroadcast(message, include_self);
}

MeshNetwork::NodeId MeshNetwork::getMyNodeId()
{
   return m_mesh.getNodeId();
}

void MeshNetwork::onReceive(receivedCallback_t receivedCallback)
{
   m_mesh.onReceive(receivedCallback);
}

set<MeshNetwork::NodeId> MeshNetwork::getNodes() { return m_nodes; }

void MeshNetwork::receivedCb(NodeId transmitterNodeId, String& msg)
{
   MY_DEBUG_PRINTF("Data received from node: %u; msg: %s\n", transmitterNodeId, msg.c_str());
   String node_prefix("NODE ");
   if(has_prefix(msg, node_prefix)) {
     String nodeid_s = remove_prefix(msg, node_prefix);
     NodeId nodeid = to_int(nodeid_s);
     if(m_nodes.count(nodeid) == 0) {
       m_history += "Node joined the mesh: " + nodeid_s;
       m_nodes.insert(nodeid);
     }
   }
}


} // namespace Facilities
