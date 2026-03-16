//------------------------------------------------------------------------------------------------
//! Sistema de teletransporte con un único destino y cooldown.
//! Compatible con servidor dedicado.
//!
//! REQUISITO: BEAR_TeleportManager.c en la misma carpeta.
class BEAR_GestorTeletransporte : ScriptedUserAction
{
	[Attribute("", UIWidgets.EditBox, "Destino", "Nombre exacto de la entidad destino en el mundo")]
	protected string destino_ViajeRapido;

	[Attribute("5", UIWidgets.Slider, "Cooldown (segundos)", "Tiempo de espera entre teletransportes", params: "0 300 1")]
	protected int tiempoRecarga;

	//------------------------------------------------------------------------------------------------
	//! PerformAction se ejecuta en el CLIENTE en servidor dedicado (HasLocalEffectOnlyScript = true).
	//! Obtenemos el PlayerController local y desde él lanzamos el RPC al servidor.
	//! Rpc() solo existe en RplComponent — SCR_PlayerController lo es, por eso funciona desde ahí.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsServer())
			return;

		if (!pUserEntity || destino_ViajeRapido.IsEmpty())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(
			GetGame().GetPlayerController()
		);

		if (!pc)
			return;

		// Llamamos al RPC directamente sobre el objeto pc, que sí extiende RplComponent
		pc.BEAR_RPC_SolicitarTeleport(destino_ViajeRapido, tiempoRecarga);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return !destino_ViajeRapido.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}