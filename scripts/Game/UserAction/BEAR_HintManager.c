//------------------------------------------------------------------------------------------------
//! Gestor de hints para teletransporte compatible con servidor dedicado.
//!
//! USA SCR_PlayerController, que ya existe en todos los jugadores vanilla.
//! NO necesitas modificar ningún prefab ni añadir componentes en el Workbench.
//!
//! INSTALACIÓN — solo tienes que hacer DOS cosas:
//!
//!   1. Copiar este archivo en la misma carpeta que los otros scripts BEAR:
//!         TuMod/Scripts/Game/Bear/BEAR_HintManager.c
//!
//!   2. Compilar en Workbench con F7 y verificar que no hay errores.
//!
//!   Eso es todo. No hay prefabs que tocar.
//!
//! CÓMO FUNCIONA:
//!   El servidor llama a EnviarHint(playerId, mensaje).
//!   Ese método obtiene el SCR_PlayerController del jugador, que Reforger
//!   ya replica automáticamente para todos los jugadores conectados.
//!   Desde ahí lanza un RPC al cliente correcto, que muestra el hint localmente.

class BEAR_HintManager
{
	//------------------------------------------------------------------------------------------------
	//! Punto de entrada desde el servidor.
	//! Llama a este método estático desde cualquier ScriptedUserAction.
	//! Ejemplo: BEAR_HintManager.EnviarHint(playerId, "Mensaje", "Título");
	static void EnviarHint(int playerId, string mensaje, string titulo = "Viaje Rápido")
	{
		// Esta clase solo debe usarse desde el servidor
		if (!Replication.IsServer())
			return;

		// Obtenemos el PlayerController del jugador destino.
		// SCR_PlayerController ya existe en todos los jugadores — no requiere prefab propio.
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			pm.GetPlayerController(playerId)
		);

		if (!playerController)
			return;

		// Delegamos el RPC al controlador del jugador
		playerController.BEAR_RpcDo_MostrarHint(mensaje, titulo);
	}
}

//------------------------------------------------------------------------------------------------
//! Extensión de SCR_PlayerController que añade el RPC de hints.
//!
//! En Reforger se puede extender una clase existente con "modded class".
//! Esto inyecta el método RPC en el PlayerController vanilla sin tocar prefabs.
//! El motor lo carga automáticamente al compilar el mod.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! RplRcver.Owner garantiza que este método se ejecuta en el cliente
	//! propietario del PlayerController, no en el servidor.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void BEAR_RpcDo_MostrarHint(string mensaje, string titulo)
	{
		// Aquí ya estamos en el cliente correcto — el hint se muestra localmente
		SCR_HintManagerComponent.ShowCustomHint(mensaje, titulo, 3.0);
	}
}