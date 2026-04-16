[ComponentEditorProps(category: "Bear")]
class BEAR_SpawnRedirectorClass : ScriptComponentClass {}

class BEAR_SpawnRedirector : ScriptComponent
{
    [Attribute("", UIWidgets.Auto, "Grupos de Spawn", "", params: "BEAR_SpawnGroup")]
    protected ref array<ref BEAR_SpawnGroup> m_aGrupos;

    protected ref map<int, string> m_mIdentidades;

    //------------------------------------------------------------------------------------------------
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);

        if (!Replication.IsServer())
            return;

        m_mIdentidades = new map<int, string>();

        int totalUIDs   = 0;
        int totalGrupos = 0;

        if (m_aGrupos)
        {
            foreach (BEAR_SpawnGroup grupo : m_aGrupos)
            {
                if (!grupo || grupo.nombreSpawnPoint.IsEmpty())
                    continue;

                grupo.Init();
                totalUIDs += grupo.CantidadUIDs();
                totalGrupos++;
                Print(string.Format("[BEAR_SpawnRedirector] Grupo '%1' cargado con %2 UIDs.",
                    grupo.nombreSpawnPoint, grupo.CantidadUIDs()), LogLevel.NORMAL);
            }
        }

        Print(string.Format("[BEAR_SpawnRedirector] %1 grupos, %2 UIDs en total.",
            totalGrupos, totalUIDs), LogLevel.NORMAL);

        SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
        if (!gm)
        {
            Print("[BEAR_SpawnRedirector] ERROR: No se encontró SCR_BaseGameMode.", LogLevel.ERROR);
            return;
        }

        gm.GetOnPlayerAuditSuccess().Insert(OnAuditSuccess);
        gm.GetOnPlayerSpawned().Insert(OnJugadorSpawned);
        gm.GetOnPlayerDisconnected().Insert(OnJugadorDesconectado);
    }

    //------------------------------------------------------------------------------------------------
    protected void OnAuditSuccess(int playerId)
    {
        string identityId = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);

        if (identityId.IsEmpty())
        {
            Print(string.Format("[BEAR_SpawnRedirector] AuditSuccess sin identityId para playerId %1",
                playerId), LogLevel.WARNING);
            return;
        }

        m_mIdentidades.Set(playerId, identityId);
        Print(string.Format("[BEAR_SpawnRedirector] Identity guardado: playerId %1 → %2",
            playerId, identityId), LogLevel.NORMAL);
    }

    //------------------------------------------------------------------------------------------------
    protected void OnJugadorSpawned(int playerId, IEntity jugador)
    {
        if (!jugador || !m_aGrupos)
            return;

        if (!m_mIdentidades.Contains(playerId))
        {
            Print(string.Format("[BEAR_SpawnRedirector] Sin identidad para playerId %1, ignorando.",
                playerId), LogLevel.WARNING);
            return;
        }

        string identityId = m_mIdentidades.Get(playerId);

        foreach (BEAR_SpawnGroup grupo : m_aGrupos)
        {
            if (!grupo || grupo.nombreSpawnPoint.IsEmpty())
                continue;

            if (!grupo.ContieneJugador(jugador))
                continue;

            string nombreDestino = grupo.ObtenerDestino(identityId);
            if (nombreDestino.IsEmpty())
            {
                Print(string.Format("[BEAR_SpawnRedirector] playerId %1 en spawn '%2' sin UID configurada.",
                    playerId, grupo.nombreSpawnPoint), LogLevel.NORMAL);
                return;
            }

            Print(string.Format("[BEAR_SpawnRedirector] Redirigiendo playerId %1 desde '%2' → '%3'",
                playerId, grupo.nombreSpawnPoint, nombreDestino), LogLevel.NORMAL);

            BEAR_TeleportData datos = new BEAR_TeleportData(
                jugador, nombreDestino, playerId, grupo.nombreSpawnPoint
            );
            GetGame().GetCallqueue().CallLater(HacerTeleport, 100, false, datos);

            return;
        }
    }

    //------------------------------------------------------------------------------------------------
    //! El teleport ahora se hace enviando un RPC al PlayerController del jugador.
    //! Ese RPC viaja servidor → cliente → servidor siguiendo el mismo patrón
    //! que BEAR_TeleportManager, garantizando que funciona en dedicado.
    protected void HacerTeleport(BEAR_TeleportData datos)
    {
        if (!datos || !datos.jugador)
        {
            Print("[BEAR_SpawnRedirector] HacerTeleport: datos nulos o jugador ya no existe.",
                LogLevel.WARNING);
            return;
        }

        // Obtenemos el SCR_PlayerController del jugador
        // que extiende RplComponent y puede enviar RPCs a su cliente propietario
        SCR_PlayerController pc = SCR_PlayerController.Cast(
            GetGame().GetPlayerManager().GetPlayerController(datos.playerId)
        );

        if (!pc)
        {
            Print(string.Format("[BEAR_SpawnRedirector] No se encontró PlayerController para playerId %1.",
                datos.playerId), LogLevel.WARNING);
            return;
        }

        // Usamos el RPC de BEAR_TeleportManager que ya sabemos que funciona en dedicado.
        // El RPC viaja al servidor con RplRcver.Server, ejecuta SetOrigin allí
        // y luego envía el hint de vuelta al cliente con RplRcver.Owner.
        pc.BEAR_RPC_EjecutarSpawnRedirect(datos.nombreDestino);

        Print(string.Format("[BEAR_SpawnRedirector] RPC de redirect enviado para playerId %1 → '%2'.",
            datos.playerId, datos.nombreDestino), LogLevel.NORMAL);
    }

    //------------------------------------------------------------------------------------------------
    protected void OnJugadorDesconectado(int playerId, KickCauseCode causa, int timeout)
    {
        if (m_mIdentidades.Contains(playerId))
        {
            m_mIdentidades.Remove(playerId);
            Print(string.Format("[BEAR_SpawnRedirector] Caché limpiada para playerId %1.", playerId),
                LogLevel.NORMAL);
        }
    }

    //------------------------------------------------------------------------------------------------
    override void OnDelete(IEntity owner)
    {
        if (Replication.IsServer())
        {
            SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
            if (gm)
            {
                gm.GetOnPlayerAuditSuccess().Remove(OnAuditSuccess);
                gm.GetOnPlayerSpawned().Remove(OnJugadorSpawned);
                gm.GetOnPlayerDisconnected().Remove(OnJugadorDesconectado);
            }
        }

        super.OnDelete(owner);
    }
}