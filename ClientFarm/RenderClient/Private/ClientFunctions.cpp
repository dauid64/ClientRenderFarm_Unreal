// Fill out your copyright notice in the Description page of Project Settings.


#include "ClientFunctions.h"
#include "MoviePipelineExecutor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/Formatters/JsonArchiveOutputFormatter.h"
#include "Serialization/StructuredArchive.h"
#include "MoviePipelineQueue.h"
#include "ObjectTools.h"
#include "MoviePipelineInProcessExecutorSettings.h"
#include "MovieRenderPipelineCoreModule.h"
#include "FileHelpers.h"
#include "MovieRenderPipelineSettings.h"
#include "PackageHelperFunctions.h"
#include "UnrealEdMisc.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "MoviePipelineGameMode.h"
#include "MoviePipelineEditorBlueprintLibrary.h"
#include "Runtime/Networking/Public/Networking.h"
#include "Runtime/Sockets/Public/Sockets.h"
#include "Runtime/Sockets/Public/SocketSubsystem.h"
#include "Runtime/Sockets/Public/SocketTypes.h"



#define LOCTEXT_NAMESPACE "UClientFunctions"

void UClientFunctions::Execute_Implementation(UMoviePipelineQueue* InPipelineQueue)
{
	Info_Render(InPipelineQueue);
	
	
}

UEditorUtilityWidget* UClientFunctions::SpawnWidget(UEditorUtilityWidgetBlueprint* blueprint, FName& tabID)
{
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	return EditorUtilitySubsystem->SpawnAndRegisterTabAndGetID(blueprint, tabID);
	
}

void UClientFunctions::CancelAllJobs_Implementation()
{
	if (!ensureMsgf(ProcessHandle.IsValid(), TEXT("Tentando cancelar o UMoviePipelineNewProcessExecutor sem uma alça valida. Isso so deve ser chamado se o processo foi originalmente valido!")))
	{
		return;
	}
	if (FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		// Process is still running, try to kill it.
		FPlatformProcess::TerminateProc(ProcessHandle);
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("UMoviePipelineNewProcessExecutor executor cancelado."))
	}
	else
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("tentando cancelar o UMoviePipelineNewProcessExecutor executor mas o processo ja saiu."));
	}
}


void UClientFunctions::CheckForProcessFinished()
{
	if (!ensureMsgf(ProcessHandle.IsValid(), TEXT("CheckForProcessFinished called without a valid process handle. This should only be called if the process was originally valid!")))
	{
		return;
	}
	int32 ReturnCode;
	if (FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
	{
		ProcessHandle.Reset();
		FCoreDelegates::OnBeginFrame.RemoveAll(this);

		// Log an error for now
		if (ReturnCode != 0)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Processo saiu com o codigo de retorno sem sucesso. Codigo de Retorno; %d"), ReturnCode);
			// OnPipelineErrored(nullptr, true, LOCTEXT("ProcessNonZeroReturn", "Non-success return code returned. See log for Return Code."));
		}

		OnExecutorFinishedImpl();
	}
	else
	{
		// Process is still running, spin wheels.
	}
}

void UClientFunctions::Info_Render(UMoviePipelineQueue* InPipelineQueue)
{
	FString GameNameOrProjectFile;
	if (FPaths::IsProjectFilePathSet())
	{
		GameNameOrProjectFile = FString::Printf(TEXT("\"%s\""), *FPaths::GetProjectFilePath());
	}
	else
	{
		GameNameOrProjectFile = FApp::GetProjectName();
	}
	/*FString MoviePipelineArgs;
	{
		
		FString PipelineConfig = TEXT("MovieRenderPipeline/QueueManifest.utxt");

		MoviePipelineArgs = FString::Printf(TEXT("-MoviePipelineConfig=\"%s\""), *PipelineConfig); // -MoviePipeline=\"%s\" -MoviePipelineLocalExecutorClass=\"%s\" -MoviePipelineClass=\"%s\""),

	}*/
	FString ManifestFilePath;
	UMoviePipelineQueue* DuplicatedQueue = UMoviePipelineEditorBlueprintLibrary::SaveQueueToManifestFile(InPipelineQueue, ManifestFilePath);
	FString ManifestoText = UMoviePipelineEditorBlueprintLibrary::ConvertManifestFileToString(ManifestFilePath);
	UClientFunctions::Socket(GameNameOrProjectFile, ManifestoText);
	return;
}

void UClientFunctions::Socket(FString NameProject, FString QueueManifest)
{
	FSocket* Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);
	FIPv4Address ip(127,0,0,1);

	TSharedRef<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(ip.Value);
	addr->SetPort(9999);

	bool connected = Socket->Connect(*addr);
	
	FString serialized = TEXT("|%|") + NameProject + TEXT("|%|") + QueueManifest + TEXT("|%|");
	TCHAR* serializedChar = serialized.GetCharArray().GetData();
	int32 size = FCString::Strlen(serializedChar);
	int32 sent = 0;
	int readLenSeq = 0;

	bool successful = Socket->Send((uint8*)TCHAR_TO_UTF8(serializedChar), size, sent);

	if (successful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mensagem Enviada"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Não foi possivel enviar a mensagem"));
		return;
	}

	uint32 BufferSize = 10000;
	TArray<uint8> ReceiveBuffer;
	FString ResultString;

	//ReceiveBuffer.Init(FMath::Min(BufferSize, 65507u), NULL);
	ReceiveBuffer.SetNumUninitialized(BufferSize);

	int32 Read = 0;
	
	//Socket->Recv(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), Read);
	if (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(1)))
	{
		Socket->Recv(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), Read);
		const FString ReceivedUE4String = StringFromBinaryArray(ReceiveBuffer);
		UE_LOG(LogTemp, Warning, TEXT("ecoado: %s"), *ReceivedUE4String);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Não Foi ecoado"));
	}
	
	//Socket->Close();
}

FString UClientFunctions::StringFromBinaryArray(TArray<uint8> BinaryArray)
{	
	BinaryArray.Add(0);
	return FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(BinaryArray.GetData())));
}

/*
void URender_Client::testJson(UMoviePipelineQueue* InPipelineQueue)
{
	FString jsonStr;
	FstructMessage textMessage;
	textMessage.InPipelineQueue = InPipelineQueue;
	textMessage.Message = "Teste";
	//FString fullPathContent = FPaths::ProjectContentDir() + fileName;
	FJsonObjectConverter::UStructToJsonObjectString(textMessage, jsonStr);

	
	UE_LOG(LogTemp, Warning, TEXT("%s"), *jsonStr);
}
*/

#undef LOCTEXT_NAMESPACE // "UClientFunctions"
