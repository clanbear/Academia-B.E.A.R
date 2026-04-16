//------------------------------------------------------------------------------------------------
//! BEAR_SpawnGroup.c
//!
//! Define un spawn point trigger y las UIDs que deben ser redirigidas
//! cuando un jugador aparece en él.
//!
//! Cada grupo representa un spawn point del mapa (ej: SP_Base_BEAR)
//! con su propia lista de jugadores y destinos.

[BaseContainerProps()]
class BEAR_SpawnGroup
{
    //! Nombre exacto de la entidad spawn point en el mundo (ej: SP_Base_BEAR)
    [Attribute("", UIWidgets.EditBox, "Nombre del spawn point trigger en el mundo")]
    string nombreSpawnPoint;

    //! Radio en metros para detectar si el jugador usó este spawn point
    [Attribute("10", UIWidgets.EditBox, "Radio de detección en metros")]
    float radioDeteccion;

    //! Lista de jugadores y sus destinos para este spawn point
    [Attribute("", UIWidgets.Auto, "UIDs y sus destinos", "", params: "BEAR_SpawnEntry")]
    ref array<ref BEAR_SpawnEntry> entradas;

    //! Caché interna: identityId → nombreEntidadDestino
    protected ref map<string, string> m_mZonas;

    //------------------------------------------------------------------------------------------------
    //! Construye la caché interna. Llamar una vez al iniciar.
    void Init()
    {
        m_mZonas = new map<string, string>();

        if (!entradas)
            return;

        foreach (BEAR_SpawnEntry entrada : entradas)
        {
            if (!entrada)
                continue;
            if (entrada.uid.IsEmpty() || entrada.nombreEntidadDestino.IsEmpty())
                continue;

            m_mZonas.Set(entrada.uid, entrada.nombreEntidadDestino);
            Print(string.Format("[BEAR_SpawnGroup] '%1' → UID %2 → entidad '%3'",
                nombreSpawnPoint, entrada.uid, entrada.nombreEntidadDestino), LogLevel.NORMAL);
        }
    }

    //------------------------------------------------------------------------------------------------
    //! Devuelve true si el jugador ha aparecido dentro del radio de este spawn point.
    bool ContieneJugador(IEntity jugador)
    {
        if (nombreSpawnPoint.IsEmpty() || !jugador)
            return false;

        IEntity spEnt = GetGame().GetWorld().FindEntityByName(nombreSpawnPoint);
        if (!spEnt)
        {
            Print(string.Format("[BEAR_SpawnGroup] AVISO: spawn point '%1' no encontrado en el mundo.",
                nombreSpawnPoint), LogLevel.WARNING);
            return false;
        }

        float dist = vector.Distance(jugador.GetOrigin(), spEnt.GetOrigin());
        return (dist <= radioDeteccion);
    }

    //------------------------------------------------------------------------------------------------
    //! Devuelve el nombre de la entidad destino para un identityId dado.
    //! Devuelve string vacío si no hay entrada para ese UID.
    string ObtenerDestino(string identityId)
    {
        if (!m_mZonas || identityId.IsEmpty())
            return string.Empty;

        if (!m_mZonas.Contains(identityId))
            return string.Empty;

        return m_mZonas.Get(identityId);
    }

    //------------------------------------------------------------------------------------------------
    int CantidadUIDs()
    {
        if (!m_mZonas)
            return 0;
        return m_mZonas.Count();
    }
}