#include "StdAfx.h"
#include "ServerPlayerSpawner.h"
#include <AzCore/Serialization/EditContext.h>
#include <AzFramework/Network/NetBindingHandlerBus.h>
#include <ISystem.h>
#include <INetwork.h>
#include <LmbrCentral/Scripting/SpawnerComponentBus.h>
#include <GridMatePlayers/PlayerBodyBus.h>

using namespace AZ;
using namespace AzFramework;
using namespace GridMate;
using namespace GridMatePlayers;

void ServerPlayerSpawner::Reflect(AZ::ReflectContext* context)
{
    if (auto sc = azrtti_cast<SerializeContext*>(context))
    {
        sc->Class<ServerPlayerSpawner, Component>()
            ->Version(1);

        if (auto ec = sc->GetEditContext())
        {
            ec->Class<ServerPlayerSpawner>(
                "Server Player Spawner",
                "Server Authoritative")
                ->ClassElement(
                    Edit::ClassElements::EditorData, "")
                ->Attribute(Edit::Attributes::Category,
                    "GridMate Players")
                ->Attribute(
                    Edit::Attributes::
                    AppearsInAddComponentMenu,
                    AZ_CRC("Game"));
        }
    }
}

void ServerPlayerSpawner::Activate()
{
    if (gEnv && gEnv->IsDedicated())
    {
        if (gEnv->pNetwork)
            SessionEventBus::Handler::BusConnect(
                gEnv->pNetwork->GetGridMate());
    }
}

void ServerPlayerSpawner::Deactivate()
{
    if (gEnv && gEnv->IsDedicated())
    {
        SessionEventBus::Handler::BusDisconnect();
    }
}

void ServerPlayerSpawner::OnMemberJoined(
    GridMate::GridSession* session,
    GridMate::GridMember* member)
{
    const auto playerId = member->GetIdCompact();
    if (session->GetMyMember()->GetIdCompact() == playerId)
        return; // ignore ourselves, the server

    const auto t = Transform::CreateTranslation(
        Vector3::CreateAxisY(10.f - m_playerCount * 1.f));
    m_playerCount++;

    AZ_Printf("GridMatePlayers", "Spawning player @ %f %f %f",
        static_cast<float>(t.GetTranslation().GetX()),
        static_cast<float>(t.GetTranslation().GetY()),
        static_cast<float>(t.GetTranslation().GetZ()));

    AzFramework::SliceInstantiationTicket ticket;
    EBUS_EVENT_ID_RESULT(ticket, GetEntityId(),
        LmbrCentral::SpawnerComponentRequestBus,
        SpawnRelative, t);

    m_joiningplayers[ticket] = playerId;
    SliceInstantiationResultBus::MultiHandler::BusConnect(
        ticket);
}

void ServerPlayerSpawner::OnSliceInstantiated(
    const AZ::Data::AssetId&,
    const SliceComponent::SliceInstanceAddress& address)
{
    const auto& ticket =
        *SliceInstantiationResultBus::GetCurrentBusId();
    const auto iter = m_joiningplayers.find(ticket);
    if (iter != m_joiningplayers.end())
    {
        const auto playerId = iter->second;
        SliceInstantiationResultBus::MultiHandler::BusDisconnect(
            ticket);

        for (auto& entity : address.second->
            GetInstantiated()->m_entities)
        {
            EBUS_EVENT_ID(entity->GetId(), PlayerBodyRequestBus,
                SetAssociatedPlayerId, playerId);
        }
    }
    m_joiningplayers.erase(iter);
}