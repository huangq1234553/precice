#include "M2N.hpp"
#include "DistributedComFactory.hpp"
#include "DistributedCommunication.hpp"
#include "com/Communication.hpp"
#include "mesh/Mesh.hpp"
#include "utils/Event.hpp"
#include "utils/MasterSlave.hpp"

using precice::utils::Event;

namespace precice
{
extern bool syncMode;

namespace m2n
{

M2N::M2N(com::PtrCommunication masterCom, DistributedComFactory::SharedPointer distrFactory)
    : _masterCom(masterCom),
      _distrFactory(distrFactory)
{
}

M2N::~M2N()
{
  if (isConnected()) {
    closeConnection();
  }
}

bool M2N::isConnected()
{
  return _isMasterConnected;
}

void M2N::acceptMasterConnection(
    const std::string &acceptorName,
    const std::string &requesterName)
{
  PRECICE_TRACE(acceptorName, requesterName);

  Event e("m2n.acceptMasterConnection", precice::syncMode);

  if (not utils::MasterSlave::isSlave()) {
    PRECICE_DEBUG("Accept master-master connection");
    PRECICE_ASSERT(_masterCom);
    _masterCom->acceptConnection(acceptorName, requesterName, utils::MasterSlave::getRank());
    _isMasterConnected = _masterCom->isConnected();
  }

  utils::MasterSlave::broadcast(_isMasterConnected);
}

void M2N::requestMasterConnection(
    const std::string &acceptorName,
    const std::string &requesterName)
{
  PRECICE_TRACE(acceptorName, requesterName);

  Event e("m2n.requestMasterConnection", precice::syncMode);

  if (not utils::MasterSlave::isSlave()) {
    PRECICE_ASSERT(_masterCom);
    PRECICE_DEBUG("Request master-master connection");
    _masterCom->requestConnection(acceptorName, requesterName, 0, 1);
    _isMasterConnected = _masterCom->isConnected();
  }

  utils::MasterSlave::broadcast(_isMasterConnected);
}

void M2N::acceptSlavesConnection(
    const std::string &acceptorName,
    const std::string &requesterName)
{
  PRECICE_TRACE(acceptorName, requesterName);
  Event e("m2n.acceptSlavesConnection", precice::syncMode);

  _areSlavesConnected = true;
  for (const auto &pair : _distComs) {
    PRECICE_DEBUG("Accept slaves-slaves connections");
    pair.second->acceptConnection(acceptorName, requesterName);
    _areSlavesConnected = _areSlavesConnected && pair.second->isConnected();
  }
  PRECICE_ASSERT(_areSlavesConnected);
}

void M2N::requestSlavesConnection(
    const std::string &acceptorName,
    const std::string &requesterName)
{
  PRECICE_TRACE(acceptorName, requesterName);
  Event e("m2n.requestSlavesConnection", precice::syncMode);

  _areSlavesConnected = true;
  for (const auto &pair : _distComs) {
    PRECICE_DEBUG("Request slaves connections");
    pair.second->requestConnection(acceptorName, requesterName);
    _areSlavesConnected = _areSlavesConnected && pair.second->isConnected();
  }
  PRECICE_ASSERT(_areSlavesConnected);
}

void M2N::prepareEstablishment()
{
  PRECICE_TRACE();
  _masterCom->prepareEstablishment();
}

void M2N::cleanupEstablishment()
{
  PRECICE_TRACE();
  _masterCom->cleanupEstablishment();
}

void M2N::acceptSlavesPreConnection(
    const std::string &acceptorName,
    const std::string &requesterName)
{
  PRECICE_TRACE(acceptorName, requesterName);
  _areSlavesConnected = true;
  for (const auto &pair : _distComs) {
    pair.second->acceptPreConnection(acceptorName, requesterName);
    _areSlavesConnected = _areSlavesConnected && pair.second->isConnected();
    }
  PRECICE_ASSERT(_areSlavesConnected);
}

void M2N::requestSlavesPreConnection(
  const std::string &acceptorName,
  const std::string &requesterName)
{
  PRECICE_TRACE(acceptorName, requesterName);
  _areSlavesConnected = true;
  for (const auto &pair : _distComs) {
    pair.second->requestPreConnection(acceptorName, requesterName);
    _areSlavesConnected = _areSlavesConnected && pair.second->isConnected();
    }
  PRECICE_ASSERT(_areSlavesConnected);
}

void M2N::completeSlavesConnection()
{
  for (const auto &pair : _distComs) {
    pair.second->updateVertexList();
  }
}

void M2N::closeConnection()
{
  PRECICE_TRACE();
  if (not utils::MasterSlave::isSlave() && _masterCom->isConnected()) {
    _masterCom->closeConnection();
    _isMasterConnected = false;
  }

  utils::MasterSlave::broadcast(_isMasterConnected);

  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    _areSlavesConnected = false;
    for (const auto &pair : _distComs) {
      pair.second->closeConnection();
      _areSlavesConnected = _areSlavesConnected || pair.second->isConnected();
    }
    PRECICE_ASSERT(not _areSlavesConnected);
  }
}

com::PtrCommunication M2N::getMasterCommunication()
{
  PRECICE_ASSERT(not utils::MasterSlave::isSlave());
  return _masterCom; /// @todo maybe it would be a nicer design to not offer this
}

void M2N::createDistributedCommunication(mesh::PtrMesh mesh)
{
  PRECICE_TRACE();
  DistributedCommunication::SharedPointer distCom = _distrFactory->newDistributedCommunication(mesh);
  _distComs[mesh->getID()]                        = distCom;
}

void M2N::send(
    double const *itemsToSend,
    int     size,
    int     meshID,
    int     valueDimension)
{
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    PRECICE_ASSERT(_areSlavesConnected);
    PRECICE_ASSERT(_distComs.find(meshID) != _distComs.end());
    PRECICE_ASSERT(_distComs[meshID].get() != nullptr);

    if (precice::syncMode) {
      if (not utils::MasterSlave::isSlave()) {
        bool ack = true;
        _masterCom->send(ack, 0);
        _masterCom->receive(ack, 0);
        _masterCom->send(ack, 0);
      }
    }
    Event e("m2n.sendData", precice::syncMode);
    _distComs[meshID]->send(itemsToSend, size, valueDimension);
  } else { //coupling mode
    PRECICE_ASSERT(_isMasterConnected);
    _masterCom->send(itemsToSend, size, 0);
  }
}

void M2N::send(bool itemToSend)
{
  PRECICE_TRACE(utils::MasterSlave::getRank());
  if (not utils::MasterSlave::isSlave()) {
    _masterCom->send(itemToSend, 0);
  }
}

void M2N::send(double itemToSend)
{
  PRECICE_TRACE(utils::MasterSlave::getRank());
  if (not utils::MasterSlave::isSlave()) {
    _masterCom->send(itemToSend, 0);
  }
}

void M2N::broadcastSendLocalMesh(mesh::Mesh &mesh)
{
  int meshID = mesh.getID();
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    PRECICE_ASSERT(_areSlavesConnected);
    PRECICE_ASSERT(_distComs.find(meshID) != _distComs.end());
    PRECICE_ASSERT(_distComs[meshID].get() != nullptr);    
    _distComs[meshID]->broadcastSendMesh();
  } else { //coupling mode
    PRECICE_ASSERT(false, "This method can only be used in parallel communication mode");
  }
}

void M2N::broadcastSendLCM(std::map<int, std::vector<int>> &localCommunicationMap,
                        mesh::Mesh &mesh)
{
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    int meshID = mesh.getID();
    PRECICE_ASSERT(_areSlavesConnected);
    _distComs[meshID]->broadcastSendLCM(localCommunicationMap);
  } else { //coupling mode
    PRECICE_ASSERT(false, "This method can only be used in parallel communication mode");
  }  
}

void M2N::broadcastSend(int &itemToSend, mesh::Mesh &mesh)
{
  int meshID = mesh.getID();
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    PRECICE_ASSERT(_areSlavesConnected);
    _distComs[meshID]->broadcastSend(itemToSend);
  } else { //coupling mode
    PRECICE_ASSERT(false, "This method can only be used with the point to point communication scheme");
  }
}

void M2N::receive(double *itemsToReceive,
                  int     size,
                  int     meshID,
                  int     valueDimension)
{
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    PRECICE_ASSERT(_areSlavesConnected);
    PRECICE_ASSERT(_distComs.find(meshID) != _distComs.end());
    PRECICE_ASSERT(_distComs[meshID].get() != nullptr);

    if (precice::syncMode) {
      if (not utils::MasterSlave::isSlave()) {
        bool ack;

        _masterCom->receive(ack, 0);
        _masterCom->send(ack, 0);
        _masterCom->receive(ack, 0);
      }
    }
    Event e("m2n.receiveData", precice::syncMode);
    _distComs[meshID]->receive(itemsToReceive, size, valueDimension);
  } else { //coupling mode
    PRECICE_ASSERT(_isMasterConnected);
    _masterCom->receive(itemsToReceive, size, 0);
  }
}

void M2N::receive(bool &itemToReceive)
{
  PRECICE_TRACE(utils::MasterSlave::getRank());
  if (not utils::MasterSlave::isSlave()) {
    _masterCom->receive(itemToReceive, 0);
  }

  utils::MasterSlave::broadcast(itemToReceive);

  PRECICE_DEBUG("receive(bool): " << itemToReceive);
}

void M2N::receive(double &itemToReceive)
{
  PRECICE_TRACE(utils::MasterSlave::getRank());
  if (not utils::MasterSlave::isSlave()) { //coupling mode
    _masterCom->receive(itemToReceive, 0);
  }

  utils::MasterSlave::broadcast(itemToReceive);

  PRECICE_DEBUG("receive(double): " << itemToReceive);
}

void M2N::broadcastReceiveAll(std::vector<int> &itemToReceive, mesh::Mesh &mesh)
{
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    int meshID = mesh.getID();
    PRECICE_ASSERT(_areSlavesConnected);
    _distComs[meshID]->broadcastReceiveAll(itemToReceive);
  } else { //coupling mode
    PRECICE_ASSERT(false, "This method can only be used with the point to point communication scheme");
  }  
}

void M2N::broadcastReceiveLocalMesh(mesh::Mesh &mesh)
{
  int meshID = mesh.getID();
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    PRECICE_ASSERT(_areSlavesConnected);
    PRECICE_ASSERT(_distComs.find(meshID) != _distComs.end());
    PRECICE_ASSERT(_distComs[meshID].get() != nullptr);
    _distComs[meshID]->broadcastReceiveMesh();
  } else { //coupling mode
    PRECICE_ASSERT(false, "This method can only be used with the point to point communication scheme");
  }
}

void M2N::broadcastReceiveLCM(std::map<int, std::vector<int>> &localCommunicationMap, mesh::Mesh &mesh)
{
  if (utils::MasterSlave::isSlave() || utils::MasterSlave::isMaster()) {
    int meshID = mesh.getID();
    PRECICE_ASSERT(_areSlavesConnected);
    _distComs[meshID]->broadcastReceiveLCM(localCommunicationMap);
  } else { //coupling mode
    PRECICE_ASSERT(false, "This method can only be used with the point to point communication scheme");
  }  
}

} // namespace m2n
} // namespace precice
