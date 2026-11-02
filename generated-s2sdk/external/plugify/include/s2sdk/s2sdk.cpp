#include "s2sdk.hpp"

s2sdk::_PlayerSlotToEntHandle __s2sdk_PlayerSlotToEntHandle = nullptr;

s2sdk::_IsClientInGame __s2sdk_IsClientInGame = nullptr;

s2sdk::_IsClientAlive __s2sdk_IsClientAlive = nullptr;

s2sdk::_GetClientGravity __s2sdk_GetClientGravity = nullptr;

s2sdk::_SetClientGravity __s2sdk_SetClientGravity = nullptr;

s2sdk::_GetClientHealth __s2sdk_GetClientHealth = nullptr;

s2sdk::_SetClientHealth __s2sdk_SetClientHealth = nullptr;

s2sdk::_GetClientPawn __s2sdk_GetClientPawn = nullptr;

s2sdk::_GetClientActiveWeapon __s2sdk_GetClientActiveWeapon = nullptr;

s2sdk::_GetClientWeapons __s2sdk_GetClientWeapons = nullptr;

s2sdk::_RemoveWeapon __s2sdk_RemoveWeapon = nullptr;

s2sdk::_GiveNamedItem __s2sdk_GiveNamedItem = nullptr;

s2sdk::_GetClientArmor __s2sdk_GetClientArmor = nullptr;

s2sdk::_SetClientArmor __s2sdk_SetClientArmor = nullptr;

s2sdk::_AddConsoleCommand __s2sdk_AddConsoleCommand = nullptr;

s2sdk::_RemoveCommand __s2sdk_RemoveCommand = nullptr;

s2sdk::_PrintToServer __s2sdk_PrintToServer = nullptr;

s2sdk::_PrintToConsole __s2sdk_PrintToConsole = nullptr;

s2sdk::_PrintToChatAll __s2sdk_PrintToChatAll = nullptr;

s2sdk::_ReplyToCommand __s2sdk_ReplyToCommand = nullptr;

s2sdk::_GetGameTickCount __s2sdk_GetGameTickCount = nullptr;

s2sdk::_GetMaxClients __s2sdk_GetMaxClients = nullptr;

s2sdk::_Precache __s2sdk_Precache = nullptr;

s2sdk::_EntPointerToEntHandle __s2sdk_EntPointerToEntHandle = nullptr;

s2sdk::_EntHandleToEntPointer __s2sdk_EntHandleToEntPointer = nullptr;

s2sdk::_IsValidEntHandle __s2sdk_IsValidEntHandle = nullptr;

s2sdk::_GetEntityClassname __s2sdk_GetEntityClassname = nullptr;

s2sdk::_GetEntityOwner __s2sdk_GetEntityOwner = nullptr;

s2sdk::_RemoveEntity __s2sdk_RemoveEntity = nullptr;

s2sdk::_HookEvent __s2sdk_HookEvent = nullptr;

s2sdk::_UnhookEvent __s2sdk_UnhookEvent = nullptr;

s2sdk::_GetEventBool __s2sdk_GetEventBool = nullptr;

s2sdk::_GetEventInt __s2sdk_GetEventInt = nullptr;

s2sdk::_GetEventPlayerSlot __s2sdk_GetEventPlayerSlot = nullptr;

s2sdk::_GetSchemaOffset __s2sdk_GetSchemaOffset = nullptr;

s2sdk::_GetEntSchema2 __s2sdk_GetEntSchema2 = nullptr;

s2sdk::_GetGameRules __s2sdk_GetGameRules = nullptr;

s2sdk::_GetAllConCommands __s2sdk_GetAllConCommands = nullptr;

s2sdk::_RemoveCommandListener __s2sdk_RemoveCommandListener = nullptr;

s2sdk::_HasConsoleCommandCallbacks __s2sdk_HasConsoleCommandCallbacks = nullptr;

s2sdk::_FindConVar __s2sdk_FindConVar = nullptr;

s2sdk::_GetConVarValue __s2sdk_GetConVarValue = nullptr;

s2sdk::_SendConVarValue __s2sdk_SendConVarValue = nullptr;

s2sdk::_SetConVarValue __s2sdk_SetConVarValue = nullptr;

s2sdk::_GetClientTeam __s2sdk_GetClientTeam = nullptr;

s2sdk::_SetEntSchema2 __s2sdk_SetEntSchema2 = nullptr;

s2sdk::_GetClientRenderColor __s2sdk_GetClientRenderColor = nullptr;

s2sdk::_SetClientRenderColor __s2sdk_SetClientRenderColor = nullptr;

s2sdk::_GetEventString __s2sdk_GetEventString = nullptr;

s2sdk::_SetEventString __s2sdk_SetEventString = nullptr;

s2sdk::_GetEntSchemaFloat2 __s2sdk_GetEntSchemaFloat2 = nullptr;

s2sdk::_SetEntSchemaFloat2 __s2sdk_SetEntSchemaFloat2 = nullptr;

s2sdk::_GetEntSchemaVector3D2 __s2sdk_GetEntSchemaVector3D2 = nullptr;

s2sdk::_SetEntSchemaVector3D2 __s2sdk_SetEntSchemaVector3D2 = nullptr;

s2sdk::_UserMessageCreateFromId __s2sdk_UserMessageCreateFromId = nullptr;

s2sdk::_UserMessageDestroy __s2sdk_UserMessageDestroy = nullptr;

s2sdk::_UserMessageSend __s2sdk_UserMessageSend = nullptr;

s2sdk::_UserMessageGetMessageID __s2sdk_UserMessageGetMessageID = nullptr;

s2sdk::_UserMessageFindMessageIdByName __s2sdk_UserMessageFindMessageIdByName = nullptr;

s2sdk::_UserMessageAddRecipient __s2sdk_UserMessageAddRecipient = nullptr;

s2sdk::_PbSetUInt32 __s2sdk_PbSetUInt32 = nullptr;

s2sdk::_PbSetFloat __s2sdk_PbSetFloat = nullptr;

s2sdk::_HookUserMessage __s2sdk_HookUserMessage = nullptr;

s2sdk::_UnhookUserMessage __s2sdk_UnhookUserMessage = nullptr;

s2sdk::_UserMessageCreateFromName __s2sdk_UserMessageCreateFromName = nullptr;

s2sdk::_UserMessageGetMessageName __s2sdk_UserMessageGetMessageName = nullptr;

s2sdk::_UserMessageAddAllPlayers __s2sdk_UserMessageAddAllPlayers = nullptr;

s2sdk::_UserMessageGetRepeatedFieldCount __s2sdk_UserMessageGetRepeatedFieldCount = nullptr;

s2sdk::_PbGetRepeatedString __s2sdk_PbGetRepeatedString = nullptr;

s2sdk::_PbAddString __s2sdk_PbAddString = nullptr;

s2sdk::_RespawnClient __s2sdk_RespawnClient = nullptr;

s2sdk::_ForceRespawnClient __s2sdk_ForceRespawnClient = nullptr;

s2sdk::_GetGameTime __s2sdk_GetGameTime = nullptr;

s2sdk::_EmitSoundToClient __s2sdk_EmitSoundToClient = nullptr;

s2sdk::_StopSound __s2sdk_StopSound = nullptr;

s2sdk::_TerminateRound __s2sdk_TerminateRound = nullptr;

s2sdk::_GetWeaponItemDefinition __s2sdk_GetWeaponItemDefinition = nullptr;

s2sdk::_GetClientSpeed __s2sdk_GetClientSpeed = nullptr;

s2sdk::_SetClientSpeed __s2sdk_SetClientSpeed = nullptr;

s2sdk::_GetClientAbsVelocity __s2sdk_GetClientAbsVelocity = nullptr;

s2sdk::_SetClientAbsVelocity __s2sdk_SetClientAbsVelocity = nullptr;

s2sdk::_GetClientEyeAngles __s2sdk_GetClientEyeAngles = nullptr;

s2sdk::_GetClientButtons __s2sdk_GetClientButtons = nullptr;

s2sdk::_GetClientFlags __s2sdk_GetClientFlags = nullptr;

s2sdk::_PrintCenterTextAll __s2sdk_PrintCenterTextAll = nullptr;

s2sdk::_PrintCentreHtmlAll __s2sdk_PrintCentreHtmlAll = nullptr;

s2sdk::_PrintToConsoleAll __s2sdk_PrintToConsoleAll = nullptr;

s2sdk::_PrintToChat __s2sdk_PrintToChat = nullptr;

s2sdk::_PrintCentreHtml __s2sdk_PrintCentreHtml = nullptr;

s2sdk::_IsFakeClient __s2sdk_IsFakeClient = nullptr;

s2sdk::_AddCommandListener __s2sdk_AddCommandListener = nullptr;

s2sdk::_GetClientAbsOrigin __s2sdk_GetClientAbsOrigin = nullptr;

s2sdk::_GetClientEyePosition __s2sdk_GetClientEyePosition = nullptr;

s2sdk::_DropWeapon __s2sdk_DropWeapon = nullptr;

s2sdk::_SelectWeapon __s2sdk_SelectWeapon = nullptr;

s2sdk::_SwitchWeapon __s2sdk_SwitchWeapon = nullptr;

s2sdk::_EntIndexToEntHandle __s2sdk_EntIndexToEntHandle = nullptr;

s2sdk::_GetFirstActiveEntity __s2sdk_GetFirstActiveEntity = nullptr;

s2sdk::_GetNextActiveEntity __s2sdk_GetNextActiveEntity = nullptr;

s2sdk::_FindEntityByClassname __s2sdk_FindEntityByClassname = nullptr;

s2sdk::_CreateEntityByName __s2sdk_CreateEntityByName = nullptr;

s2sdk::_DispatchSpawn __s2sdk_DispatchSpawn = nullptr;

s2sdk::_DispatchSpawn2 __s2sdk_DispatchSpawn2 = nullptr;

s2sdk::_GetEntityMoveType __s2sdk_GetEntityMoveType = nullptr;

s2sdk::_SetEntityMoveType __s2sdk_SetEntityMoveType = nullptr;

s2sdk::_SetEntityGravity __s2sdk_SetEntityGravity = nullptr;

s2sdk::_SetEntityOwner __s2sdk_SetEntityOwner = nullptr;

s2sdk::_GetEntityParent __s2sdk_GetEntityParent = nullptr;

s2sdk::_SetEntityParent __s2sdk_SetEntityParent = nullptr;

s2sdk::_SetEntityParentAttachment __s2sdk_SetEntityParentAttachment = nullptr;

s2sdk::_GetEntityAbsOrigin __s2sdk_GetEntityAbsOrigin = nullptr;

s2sdk::_SetEntityAbsOrigin __s2sdk_SetEntityAbsOrigin = nullptr;

s2sdk::_GetEntityLocalOrigin __s2sdk_GetEntityLocalOrigin = nullptr;

s2sdk::_SetEntityLocalOrigin __s2sdk_SetEntityLocalOrigin = nullptr;

s2sdk::_GetEntityAbsVelocity __s2sdk_GetEntityAbsVelocity = nullptr;

s2sdk::_SetEntityAbsVelocity __s2sdk_SetEntityAbsVelocity = nullptr;

s2sdk::_SetEntityAngVelocity __s2sdk_SetEntityAngVelocity = nullptr;

s2sdk::_SetEntityModel __s2sdk_SetEntityModel = nullptr;

s2sdk::_TeleportEntity __s2sdk_TeleportEntity = nullptr;

s2sdk::_AcceptEntityInput __s2sdk_AcceptEntityInput = nullptr;

s2sdk::_GetEntityAttachmentAngles __s2sdk_GetEntityAttachmentAngles = nullptr;

s2sdk::_GetEntityAttachmentOrigin __s2sdk_GetEntityAttachmentOrigin = nullptr;

s2sdk::_GetEntityModelScale __s2sdk_GetEntityModelScale = nullptr;

s2sdk::_SetEntityModelScale __s2sdk_SetEntityModelScale = nullptr;

s2sdk::_ScriptLookupAttachment __s2sdk_ScriptLookupAttachment = nullptr;

s2sdk::_GetEntSchemaEnt2 __s2sdk_GetEntSchemaEnt2 = nullptr;

s2sdk::_SetEntSchemaEnt2 __s2sdk_SetEntSchemaEnt2 = nullptr;

s2sdk::_OnClientDisconnect_Register __s2sdk_OnClientDisconnect_Register = nullptr;

s2sdk::_OnClientDisconnect_Unregister __s2sdk_OnClientDisconnect_Unregister = nullptr;

s2sdk::_OnBuildGameSessionManifest_Register __s2sdk_OnBuildGameSessionManifest_Register = nullptr;

s2sdk::_OnBuildGameSessionManifest_Unregister __s2sdk_OnBuildGameSessionManifest_Unregister = nullptr;

s2sdk::_IsClientAuthorized __s2sdk_IsClientAuthorized = nullptr;

s2sdk::_GetClientSteamID64 __s2sdk_GetClientSteamID64 = nullptr;

s2sdk::_GetClientLanguageCode __s2sdk_GetClientLanguageCode = nullptr;

s2sdk::_GetEntityAngRotation __s2sdk_GetEntityAngRotation = nullptr;

s2sdk::_QueryInterface __s2sdk_QueryInterface = nullptr;

s2sdk::_LoadGameConfigFile __s2sdk_LoadGameConfigFile = nullptr;

s2sdk::_CloseGameConfigFile __s2sdk_CloseGameConfigFile = nullptr;

s2sdk::_GetGameConfigOffset __s2sdk_GetGameConfigOffset = nullptr;

s2sdk::_GetGameConfigSignature __s2sdk_GetGameConfigSignature = nullptr;

s2sdk::_GetGameConfigVTable __s2sdk_GetGameConfigVTable = nullptr;

s2sdk::_IsEntityPlayerPawn __s2sdk_IsEntityPlayerPawn = nullptr;

s2sdk::_IsEntityPlayerController __s2sdk_IsEntityPlayerController = nullptr;

s2sdk::_GetEntityEyePosition __s2sdk_GetEntityEyePosition = nullptr;

s2sdk::_GetEntityEyeAngles __s2sdk_GetEntityEyeAngles = nullptr;

s2sdk::_GetHiddenTransmitEntities __s2sdk_GetHiddenTransmitEntities = nullptr;

s2sdk::_HideTransmitEntity __s2sdk_HideTransmitEntity = nullptr;

s2sdk::_ShowTransmitEntity __s2sdk_ShowTransmitEntity = nullptr;

s2sdk::_NetworkStateChanged __s2sdk_NetworkStateChanged = nullptr;

s2sdk::_OnMapStart_Register __s2sdk_OnMapStart_Register = nullptr;

s2sdk::_OnMapStart_Unregister __s2sdk_OnMapStart_Unregister = nullptr;

s2sdk::_OnMapEnd_Register __s2sdk_OnMapEnd_Register = nullptr;

s2sdk::_OnMapEnd_Unregister __s2sdk_OnMapEnd_Unregister = nullptr;

s2sdk::_GetClientKills __s2sdk_GetClientKills = nullptr;

s2sdk::_GetClientDeaths __s2sdk_GetClientDeaths = nullptr;

s2sdk::_GetEventFloat __s2sdk_GetEventFloat = nullptr;

s2sdk::_CreateConVarBool __s2sdk_CreateConVarBool = nullptr;

s2sdk::_ChangeClientTeam __s2sdk_ChangeClientTeam = nullptr;

s2sdk::_FakeClientCommand __s2sdk_FakeClientCommand = nullptr;

s2sdk::_GetCurrentMap __s2sdk_GetCurrentMap = nullptr;

s2sdk::_GetGameDirectory __s2sdk_GetGameDirectory = nullptr;

