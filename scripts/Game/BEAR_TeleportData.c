//! Contenedor de datos para pasar al CallLater de HacerTeleport.
class BEAR_TeleportData
{
    IEntity jugador;
    string  nombreDestino;
    int     playerId;
    string  spawnOrigen;

    void BEAR_TeleportData(IEntity j, string dest, int pid, string origen)
    {
        jugador        = j;
        nombreDestino  = dest;
        playerId       = pid;
        spawnOrigen    = origen;
    }
}