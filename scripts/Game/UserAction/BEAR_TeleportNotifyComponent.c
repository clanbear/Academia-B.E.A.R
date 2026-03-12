//------------------------------------------------------------------------------------------------
//! Componente auxiliar para enviar hints desde el servidor a un cliente específico.
//! 
//! PASO 1 — Añadir este archivo a tu proyecto:
//!   Copia este archivo en la misma carpeta que tus otros scripts BEAR.
//!   Ejemplo: Scripts/Game/Bear/BEAR_TeleportNotifyComponent.c
//!
//! PASO 2 — Añadir el componente al prefab del jugador (en Workbench):
//!   1. Busca el prefab de tu jugador (ej. Character_US_Base.et)
//!   2. Clic derecho → "Create Inherited Prefab" para no modificar el original
//!   3. En el nuevo prefab, pulsa "+" en el panel de componentes
//!   4. Busca "BEAR_TeleportNotifyComponent" y añádelo
//!   5. Guarda con Ctrl+S y usa este prefab en tu misión
//!
//! PASO 3 — Compilar y verificar:
//!   1. En Workbench abre Script Editor y pulsa F7 (Compile)
//!   2. Comprueba que no hay errores
//!   3. Verifica que el componente aparece en el buscador de componentes

[ComponentEditorProps(category: "Bear")]
class BEAR_TeleportNotifyComponentClass : ScriptComponentClass {}

class BEAR_TeleportNotifyComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! Se ejecuta en el CLIENTE propietario del personaje gracias a RplRcver.Owner.
	//! Nunca llames a este método directamente — usa MostrarHintDesdeServidor().
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MostrarHint(string mensaje, string titulo)
	{
		SCR_HintManagerComponent.ShowCustomHint(mensaje, titulo, 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Llama a este método desde el servidor para enviar un hint al cliente propietario.
	void MostrarHintDesdeServidor(string mensaje, string titulo)
	{
		Rpc(RpcDo_MostrarHint, mensaje, titulo);
	}
}