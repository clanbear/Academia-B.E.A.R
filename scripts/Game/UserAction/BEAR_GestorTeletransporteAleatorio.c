//------------------------------------------------------------------------------------------------
//! Sistema de teletransporte aleatorio entre múltiples destinos configurables
//! Ideal para cursos de orientación: el jugador es enviado a una zona aleatoria
class BEAR_GestorTeletransporteAleatorio : ScriptedUserAction
{
	//! Desplazamiento mínimo respecto al destino para evitar colisiones
	protected const float DESPLAZAMIENTO_TELEPORT = 1.0;
	
	//! Distancia mínima entre jugador y destino para permitir teletransporte
	protected const float DISTANCIA_MINIMA_TELEPORT = 1.5;

	[Attribute("10", UIWidgets.Slider, "Cooldown (segundos)", "Tiempo de espera entre teletransportes por jugador", params: "0 300 1")]
	protected int tiempoRecarga;

	//! Lista de nombres de entidades destino (se elige una aleatoriamente)
	[Attribute("", UIWidgets.Auto, "Destinos posibles", "Lista de entidades destino. Se elegirá una aleatoriamente al usarlo.")]
	protected ref array<string> listaDestinos;

	//! Mapa para controlar el cooldown por jugador (PlayerID -> en cooldown?)
	protected ref map<int, bool> mapaRecargaJugadores;
	
	//! Mapa con timestamp del último uso por jugador (PlayerID -> tiempo en ms)
	protected ref map<int, float> mapaUltimoUso;

	//------------------------------------------------------------------------------------------------
	//! Constructor - inicializa el sistema de cooldown y la lista de destinos
	void BEAR_GestorTeletransporteAleatorio()
	{
		mapaRecargaJugadores = new map<int, bool>();
		mapaUltimoUso = new map<int, float>();
		
		if (!listaDestinos)
			listaDestinos = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	//! Ejecuta la acción de teletransporte aleatorio con validaciones y cooldown
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!ValidarEntidades(pOwnerEntity, pUserEntity))
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);

		// Verificar cooldown del jugador
		if (JugadorEnCooldown(playerId))
		{
			float tiempoRestante = ObtenerCooldownRestante(playerId);
			int segundosRestantes = Math.Ceil(tiempoRestante);
			MostrarSugerenciaAJugador(pUserEntity, string.Format("Espera %1 segundos", segundosRestantes));
			return;
		}

		// Verificar que haya destinos configurados
		if (!listaDestinos || listaDestinos.IsEmpty())
		{
			MostrarSugerenciaAJugador(pUserEntity, "No hay destinos configurados.");
			return;
		}

		// Elegir destino aleatorio y ejecutar teletransporte
		string destinoElegido = ElegirDestinoAleatorio(pUserEntity);
		
		if (destinoElegido.IsEmpty())
		{
			MostrarSugerenciaAJugador(pUserEntity, "No se encontró ningún destino válido.");
			return;
		}

		if (TeletransportarJugador(pUserEntity, destinoElegido))
		{
			IniciarCooldown(playerId);
			MostrarSugerenciaAJugador(pUserEntity, string.Format("Transportando a: %1", ObtenerNombreAccion()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Elige un destino aleatorio de la lista, priorizando destinos válidos (que existan en el mundo)
	protected string ElegirDestinoAleatorio(IEntity player)
	{
		// Construir lista de destinos válidos (entidades que existen en el mundo)
		array<string> destinosValidos = new array<string>();
		
		foreach (string nombre : listaDestinos)
		{
			if (nombre.IsEmpty())
				continue;
				
			IEntity entidad = GetGame().GetWorld().FindEntityByName(nombre);
			if (!entidad)
				continue;
			
			// Verificar que la distancia sea suficiente para teletransportar
			float distancia = vector.Distance(player.GetOrigin(), entidad.GetOrigin());
			if (distancia < DISTANCIA_MINIMA_TELEPORT)
				continue;
				
			destinosValidos.Insert(nombre);
		}
		
		if (destinosValidos.IsEmpty())
			return string.Empty;
		
		// Elegir índice aleatorio
		int indiceAleatorio = Math.RandomInt(0, destinosValidos.Count());
		return destinosValidos[indiceAleatorio];
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidarEntidades(IEntity owner, IEntity user)
	{
		return owner && user;
	}

	//------------------------------------------------------------------------------------------------
	protected bool JugadorEnCooldown(int playerId)
	{
		return mapaRecargaJugadores.Contains(playerId) && mapaRecargaJugadores.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TeletransportarJugador(IEntity player, string nombreDestino)
	{
		IEntity entidadDestino = GetGame().GetWorld().FindEntityByName(nombreDestino);

		if (!entidadDestino)
			return false;

		vector posJugador = player.GetOrigin();
		vector posDestino = entidadDestino.GetOrigin();

		// Calcular posición final con offset para evitar colisiones
		vector direccionOffset = (posJugador - posDestino).Normalized();
		vector posicionFinal = posDestino + (direccionOffset * DESPLAZAMIENTO_TELEPORT);

		player.SetOrigin(posicionFinal);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void IniciarCooldown(int playerId)
	{
		if (tiempoRecarga <= 0)
			return;

		mapaRecargaJugadores.Set(playerId, true);
		mapaUltimoUso.Set(playerId, GetGame().GetWorld().GetWorldTime());

		GetGame().GetCallqueue().CallLater(RestablecerCooldown, tiempoRecarga * 1000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestablecerCooldown(int playerId)
	{
		mapaRecargaJugadores.Set(playerId, false);
	}
	
	//------------------------------------------------------------------------------------------------
	protected float ObtenerCooldownRestante(int playerId)
	{
		if (!mapaUltimoUso.Contains(playerId))
			return 0;
			
		float ultimoUso = mapaUltimoUso.Get(playerId);
		float tiempoActual = GetGame().GetWorld().GetWorldTime();
		float transcurrido = (tiempoActual - ultimoUso) / 1000.0;
		float restante = tiempoRecarga - transcurrido;
		
		if (restante < 0)
			return 0;
			
		return restante;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void MostrarSugerenciaAJugador(IEntity player, string mensaje)
	{
		if (!player)
			return;
			
		SCR_HintManagerComponent.ShowCustomHint(mensaje, "Viaje Rápido", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Obtiene el nombre configurado en UI Info
	protected string ObtenerNombreAccion()
	{
	    UIInfo uiInfo = GetUIInfo();
	    if (!uiInfo)
	        return string.Empty;
	        
	    return uiInfo.GetName();
	}
}