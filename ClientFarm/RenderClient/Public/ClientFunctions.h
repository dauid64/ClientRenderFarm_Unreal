// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineExecutor.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityTask.h"
#include "EditorUtilitySubsystem.h"
#include "Runtime/Engine/Classes/Kismet/BlueprintFunctionLibrary.h"

#include "ClientFunctions.generated.h"


/**
 * 
 */
UCLASS()
class RENDERCLIENT_API UClientFunctions : public UMoviePipelineExecutorBase 
{
	GENERATED_BODY()
	UFUNCTION(CallInEditor, BlueprintCallable)
	virtual void Execute_Implementation(UMoviePipelineQueue* InPipelineQueue) override;

	UFUNCTION(CallInEditor, BlueprintCallable)
	UEditorUtilityWidget* SpawnWidget(class UEditorUtilityWidgetBlueprint* blueprint, FName& tabID);
	

	virtual bool IsRendering_Implementation() const override { return ProcessHandle.IsValid(); }

	virtual void CancelCurrentJob_Implementation() override { CancelAllJobs_Implementation(); }
	virtual void CancelAllJobs_Implementation() override;

	UFUNCTION(BlueprintCallable)
		void Socket(FString NameProject, FString QueueManifest);

public:
	//UFUNCTION(BlueprintCallable)
		//void testJson(UMoviePipelineQueue* InPipelineQueue);
protected:
	void CheckForProcessFinished();

	
	UFUNCTION(BlueprintCallable)
		void Info_Render(UMoviePipelineQueue* InPipelineQueue);

	FString StringFromBinaryArray(const TArray<uint8> BinaryArray);

protected:
	FProcHandle ProcessHandle;
};

