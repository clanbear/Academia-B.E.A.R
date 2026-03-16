//------------------------------------------------------------------------------------------------
//! Sistema de teletransporte aleatorio entre múltiples destinos.
//! Compatible con servidor dedicado.
//!
//! REQUISITO: BEAR_TeleportManager.c en la misma carpeta.
class BEAR_GestorTeletransporteAleatorio : ScriptedUserAction
{
	[Attribute("5", UIWidgets.Slider, "Cooldown (segundos)", "Tiempo de espera entre teletransportes", params: "0 300 1")]
	protected int tiempoRecarga;

	[Attribute("", UIWidgets.Auto, "Destinos posibles", "Lista de entidades destino. Se elegirá una aleatoriamente.")]
	protected ref array<string> listaDestinos;

	//------------------------------------------------------------------------------------------------
	void BEAR_GestorTeletransporteAleatorio()
	{
		if (!listaDestinos)
			listaDestinos = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	//! PerformAction se ejecuta en el CLIENTE en servidor dedicado (HasLocalEffectOnlyScript = true).
	//! Serializamos la lista manualmente (string.Join no existe en EnforceScript)
	//! y llamamos al RPC desde el PlayerController, que sí extiende RplComponent.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsServer())
			return;

		if (!pUserEntity || !listaDestinos || listaDestinos.IsEmpty())
			return;

		// Serializar lista manualmente con separador "|"
		// Usamos "|" en lugar de "," por si algún nombre tuviera comas
		string destinosSerializados = "";
		foreach (int i, string nombre : listaDestinos)
		{
			if (nombre.IsEmpty())
				continue;
			if (destinosSerializados != "")
				destinosSerializados += "|";
			destinosSerializados += nombre;
		}

		if (destinosSerializados.IsEmpty())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(
			GetGame().GetPlayerController()
		);

		if (!pc)
			return;

		// Llamamos al RPC directamente sobre el objeto pc, que sí extiende RplComponent
		pc.BEAR_RPC_SolicitarTeleportAleatorio(destinosSerializados, tiempoRecarga);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return listaDestinos && !listaDestinos.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}