/*****************************************************************************
 *                 - Copyright (C) - 2022 - InfinyTech3D -                   *
 *                                                                           *
 * This file is part of the SofaUE5-Renderer asset from InfinyTech3D         *
 *                                                                           *
 * GNU General Public License Usage:                                         *
 * This file may be used under the terms of the GNU General                  *
 * Public License version 3. The licenses are as published by the Free       *
 * Software Foundation and appearing in the file LICENSE.GPL3 included in    *
 * the packaging of this file. Please review the following information to    *
 * ensure the GNU General Public License requirements will be met:           *
 * https://www.gnu.org/licenses/gpl-3.0.html.                                *
 *                                                                           *
 * Commercial License Usage:                                                 *
 * Licensees holding valid commercial license from InfinyTech3D may use this *
 * file in accordance with the commercial license agreement provided with    *
 * the Software or, alternatively, in accordance with the terms contained in *
 * a written agreement between you and InfinyTech3D. For further information *
 * on the licensing terms and conditions, contact: contact@infinytech3d.com  *
 *                                                                           *
 * Authors: see Authors.txt                                                  *
 * Further information: https://infinytech3d.com                             *
 ****************************************************************************/
#pragma once

#include "GameFramework/Actor.h"
#include "SofaContext.generated.h"

class SofaPhysicsAPI;
class SofaPhysicsOutputMesh;
class UStaticMeshComponent;

UCLASS()
class SOFAUE5_API ASofaContext : public AActor
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void BeginDestroy() override;

public:
    ASofaContext();

    virtual void PostActorCreated() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    virtual bool ShouldTickIfViewportsOnly() const override { return false; }
    virtual bool IsLevelBoundsRelevant() const override { return false; }

    // Called every frame
    virtual void Tick( float DeltaSeconds ) override;


    void setDT(float value);

    void setGravity(FVector value);

    SofaPhysicsAPI* getSofaAPI() { return m_sofaAPI; }

    class SofaPhysicsOutputMesh* getOutputMeshByName(const FString& name);

    bool isSceneLoaded() const { return m_status > 0; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sofa Parameters")
        FFilePath filePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sofa Parameters")
        float Dt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sofa Parameters")
        FVector Gravity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sofa Parameters")
        bool m_isMsgHandlerActivated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sofa Parameters")
        bool m_log = false;

    // Floor Detection
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Settings",
        meta = (Tooltip = "Sample floor/terrain on a grid and inject as triangle collision. Works for flat or uneven ground. Name actors 'Floor' or 'Ground' (or add to substrings)."))
        bool bAutoDetectFloor = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Settings",
        meta = (Tooltip = "Size of floor area to sample (cm). Centered on SofaContext.", EditCondition = "bAutoDetectFloor", ClampMin = "500.0", ClampMax = "50000.0"))
        float FloorGridSize = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Settings",
        meta = (Tooltip = "Grid divisions (higher = more detail). Landscapes: set Collision Mip Level to 0 (Details > Collision).", EditCondition = "bAutoDetectFloor", ClampMin = "2", ClampMax = "50"))
        int32 FloorGridDivisions = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor Settings",
        meta = (Tooltip = "Actor or mesh name must contain one of these (case insensitive). Empty = floor, ground.", EditCondition = "bAutoDetectFloor"))
        TArray<FString> FloorActorNameSubstrings;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Floor Settings",
        meta = (Tooltip = "Delete processed scene and floor collision cache in Saved/SofaTemp. Use before debugging so the next Play regenerates them."))
    void ClearSceneCache();

protected:
    void catchSofaMessages();

    void createSofaContext();

    void loadDefaultPlugin();

    /** Auto-spawn SofaVisualMesh actors for all SOFA output meshes */
    void SpawnVisualMeshActors();

    /** Check if there are existing SofaVisualMesh actors referencing this context */
    bool HasExistingVisualMeshes();

    /** Return true if actor (or its static mesh name) matches FloorActorNameSubstrings */
    bool IsActorFloor(AActor* Actor, UStaticMeshComponent* MeshComp) const;

    /** Process scene file to inject or modify PlaneForceField for floor */
    FString ProcessSceneForFloor(const FString& OriginalPath);

    /** Find all SofaCollisionMesh actors and inject their geometry into the scene */
    FString InjectCollisionMeshes(const FString& SceneContent);

    /** Extract floor mesh geometry within a radius and export to OBJ file */
    FString ExportFloorCollisionMesh();

    /** Generate SOFA XML for mesh-based floor collision */
    FString GenerateFloorMeshCollisionXML(const FString& ObjFilePath);

private:
    int32 m_dllLoadStatus;
    FString m_apiName;
    bool m_isInit;
    
    FString CachedFloorObjPath;
    FVector CachedFloorLocation;
    
    SofaPhysicsAPI* m_sofaAPI = nullptr;
    int m_status;
};
