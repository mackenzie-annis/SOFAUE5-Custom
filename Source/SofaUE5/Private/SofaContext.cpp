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
#include "SofaContext.h"
#include "SofaUE5.h"
#include "Engine.h"
#include "CoreMinimal.h"
#include "SofaVisualMesh.h"
#include "SofaCollisionMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Misc/FileHelper.h"
#include "XmlFile.h"
#include <vector>
#include <string>
#include <regex>

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "SofaUE5Library/SofaPhysicsAPI.h"

 // Sets default values
ASofaContext::ASofaContext()
    : Dt(0.02)
    , Gravity(0, -9.8, 0)
    , m_isMsgHandlerActivated(true)
    , m_dllLoadStatus(0)
    , m_apiName("")
    , m_isInit(false)
    , m_sofaAPI(nullptr)
    , m_status(-1)
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SofaContext"));
    SetActorScale3D(FVector(-10.0, 10.0, 10.0));
    SetActorRotation(FRotator(0.0, 0.0, 270.0));

    if (m_log)
        UE_LOG(SUnreal_log, Warning, TEXT("######### ASofaContext::ASofaContext(): %s | %s ##########"), *this->GetName(), *intToHexa(this->GetFlags()));
}

void ASofaContext::PostActorCreated()
{
    if (m_log)
        UE_LOG(SUnreal_log, Warning, TEXT("######### ASofaContext::PostActorCreated(): %s | %s ##########"), *this->GetName(), *intToHexa(this->GetFlags()));

    // Don't load scene here - wait for BeginPlay or when user sets filePath
    // This prevents duplicate loading and phantom meshes
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] PostActorCreated - waiting for Play or filePath change"));
}

// Called when the game starts or when spawned
void ASofaContext::BeginPlay()
{
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] BeginPlay() called for %s"), *GetName());
    createSofaContext();

    if (m_sofaAPI)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Starting SOFA simulation..."));
        m_sofaAPI->start();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] m_sofaAPI is null in BeginPlay"));
    }

    Super::BeginPlay();
}


void ASofaContext::setDT(float value)
{
    if (m_sofaAPI)
        m_sofaAPI->setTimeStep(value);
}

void ASofaContext::setGravity(FVector value)
{
    if (m_sofaAPI)
    {
        UE_LOG(SUnreal_log, Warning, TEXT("## ASofaContext::setGravity: %f, %f, %f"), value.X, value.Y, value.Z);
        double* grav = new double[3];
        grav[0] = value.X;
        grav[1] = value.Y;
        grav[2] = value.Z;
        m_sofaAPI->setGravity(grav);
    }
}

void ASofaContext::BeginDestroy()
{
    if (m_log)
        UE_LOG(SUnreal_log, Warning, TEXT("######### ASofaContext::BeginDestroy(): %s | %s ##########"), *this->GetName(), *intToHexa(this->GetFlags()));

    if (m_sofaAPI)
    {
        if (m_log)
            UE_LOG(SUnreal_log, Warning, TEXT("######### ASofaContext::BeginDestroy(): Delete SofaAdvancePhysicsAPI: %s"), *this->GetName());

        m_sofaAPI->stop();
        delete m_sofaAPI;
        m_sofaAPI = NULL;
    }

    Super::BeginDestroy();
}

void ASofaContext::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (m_log)
        UE_LOG(SUnreal_log, Warning, TEXT("######### ASofaContext::EndPlay(): %s | %s ##########"), *this->GetName(), *intToHexa(this->GetFlags()));

    if (m_sofaAPI)
    {
        m_sofaAPI->stop();
        m_sofaAPI->activateMessageHandler(false);
    }
    Super::EndPlay(EndPlayReason);
}



#if WITH_EDITOR
void ASofaContext::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    if (PropertyChangedEvent.MemberProperty != nullptr)
    {
        FString MemberName = PropertyChangedEvent.MemberProperty->GetName();

        if (MemberName.Compare(TEXT("Gravity")) == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Gravity is %s"), *Gravity.ToString());
            setGravity(Gravity);
        }
        else if (MemberName.Compare(TEXT("Dt")) == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Dt is %f"), Dt);
            setDT(Dt);
        }
        else if (MemberName.Compare(TEXT("filePath")) == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA] FilePath changed to: %s - reloading scene"), *filePath.FilePath);
            createSofaContext();
            
            // Auto-spawn visual meshes if none exist for this context
            if (m_status > 0 && !HasExistingVisualMeshes())
            {
                UE_LOG(LogTemp, Warning, TEXT("[SOFA] No existing visual meshes found, auto-spawning in editor..."));
                SpawnVisualMeshActors();
            }
        }
    }
}
#endif


// Called every frame
void ASofaContext::Tick(float DeltaTime)
{
    if (m_status != -1 && m_sofaAPI)
    {
        m_sofaAPI->step();

        if (m_isMsgHandlerActivated == true)
            catchSofaMessages();
    }

    Super::Tick(DeltaTime);
}


void ASofaContext::createSofaContext()
{
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] createSofaContext() called"));

    FString curPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] ProjectDir = %s"), *curPath);

    // Step 1. If we already have a SOFA API, destroy it completely and start fresh
    if (m_sofaAPI != nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Destroying previous SofaPhysicsAPI and creating fresh one..."));
        m_sofaAPI->stop();
        delete m_sofaAPI;
        m_sofaAPI = nullptr;
        m_status = -1;
    }

    // Step 2. Create API
    if (m_sofaAPI == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] m_sofaAPI is null, creating new SofaPhysicsAPI..."));
        m_sofaAPI = new SofaPhysicsAPI(false);

        if (m_sofaAPI == nullptr)
        {
            UE_LOG(LogTemp, Error, TEXT("[SOFA] FAILED to create SofaPhysicsAPI instance!"));
            return;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA] Successfully created SofaPhysicsAPI"));
        }

        m_sofaAPI->activateMessageHandler(m_isMsgHandlerActivated);
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Message handler activated = %s"), m_isMsgHandlerActivated ? TEXT("true") : TEXT("false"));

        // Only load plugins once when creating the API
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Loading default plugins..."));
        loadDefaultPlugin();
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Finished loading plugins."));
    }

    // Step 3. Check file path
    if (filePath.FilePath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] No filePath set for the scene."));
        return;
    }

    FString my_filePath = FPaths::ConvertRelativePathToFull(filePath.FilePath);
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Scene path: %s"), *my_filePath);

    // Check if scene file exists
    if (!FPaths::FileExists(my_filePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] Scene file does not exist: %s"), *my_filePath);
        return;
    }

    // Process scene for floor detection/injection
    FString ProcessedPath = ProcessSceneForFloor(my_filePath);
    if (ProcessedPath != my_filePath)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Using processed scene with floor: %s"), *ProcessedPath);
        my_filePath = ProcessedPath;
    }

    // Test if API is properly initialized
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Checking API impl pointer: %p"), m_sofaAPI->impl);
    if (m_sofaAPI->impl == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] CRITICAL: SofaPhysicsAPI impl is NULL! API not properly initialized."));
        return;
    }
    const char* apiName = m_sofaAPI->APIName();
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] API Name: %s"), apiName ? ANSI_TO_TCHAR(apiName) : TEXT("(null)"));

    // Save current working directory
    TCHAR OriginalDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, OriginalDir);
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Original working directory: %s"), OriginalDir);

    // Change to scene directory so SOFA can resolve relative mesh paths
    FString SceneDir = FPaths::ConvertRelativePathToFull(FPaths::GetPath(my_filePath));
    SetCurrentDirectory(*SceneDir);
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Changed working directory to: %s"), *SceneDir);

    UE_LOG(LogTemp, Warning, TEXT("[SOFA] About to call m_sofaAPI->load() with path: %s"), *my_filePath);
    const char* pathfile = TCHAR_TO_ANSI(*my_filePath);
    int resScene = m_sofaAPI->load(pathfile);

    // Restore original working directory
    SetCurrentDirectory(OriginalDir);
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Restored working directory to: %s"), OriginalDir);

    UE_LOG(LogTemp, Warning, TEXT("[SOFA] load() result = %d"), resScene);

    if (resScene <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] Scene loading failed!"));
        return;
    }

    // Mark scene as successfully loaded
    m_status = resScene;

    // Step 4. Check number of meshes
    unsigned int nbr = m_sofaAPI->getNbOutputMeshes();
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] NbOutputMeshes = %d"), nbr);

    if (nbr == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] No meshes returned from SOFA (this may be OK for some scenes)."));
    }

    for (unsigned int meshID = 0; meshID < nbr; meshID++)
    {
        SofaPhysicsOutputMesh* mesh = m_sofaAPI->getOutputMeshPtr(meshID);

        if (mesh == nullptr)
        {
            UE_LOG(LogTemp, Error, TEXT("[SOFA] getOutputMeshPtr(%d) returned NULL"), meshID);
            continue;
        }

        const char* name = mesh->getName();
        FString FName(name);
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Mesh %d name: %s"), meshID, *FName);
    }

    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Finished createSofaContext() with status = %d"), m_status);

    // Auto-spawn visual mesh actors if no existing ones are set up for this context
    if (m_status > 0 && !HasExistingVisualMeshes())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] No existing visual meshes found, auto-spawning..."));
        SpawnVisualMeshActors();
    }
    else if (m_status > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Existing visual meshes found, skipping auto-spawn"));
    }
}

void ASofaContext::loadDefaultPlugin()
{
    if (m_sofaAPI == nullptr)
        return;

    // Get the plugin base directory dynamically using the plugin manager
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("SofaUE5");
    if (!Plugin.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] Failed to find SofaUE5 plugin!"));
        return;
    }
    
    FString BaseDir = Plugin->GetBaseDir();
    const FString PluginDir = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/SofaUE5Library/Win64"))
    );
    FPlatformProcess::AddDllDirectory(*PluginDir);
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] PluginDir (DLL search path) = %s"), *PluginDir);

    // Load the sofa.ini configuration file
    FString IniPath = FPaths::Combine(*PluginDir, TEXT("sofa.ini"));
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Loading sofa.ini from: %s"), *IniPath);
    const char* sharePath = m_sofaAPI->loadSofaIni(TCHAR_TO_ANSI(*IniPath));
    if (sharePath)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] sofa.ini loaded, share path: %s"), ANSI_TO_TCHAR(sharePath));
    }

    // Load plugins using FULL PATH to DLL files
    auto LoadPluginWithPath = [&](const FString& PluginName) {
        FString FullPath = FPaths::Combine(*PluginDir, PluginName + TEXT(".dll"));
        int result = m_sofaAPI->loadPlugin(TCHAR_TO_ANSI(*FullPath));
        if (result != 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA] Failed to load plugin %s, error code: %d"), *PluginName, result);
        }
        return result;
    };

    // Read and load all plugins from plugin_list.conf (like runSofa does)
    FString PluginListPath = FPaths::Combine(*PluginDir, TEXT("plugin_list.conf"));
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Loading plugins from: %s"), *PluginListPath);
    
    FString PluginListContent;
    if (FFileHelper::LoadFileToString(PluginListContent, *PluginListPath))
    {
        TArray<FString> Lines;
        PluginListContent.ParseIntoArrayLines(Lines);
        
        int loadedCount = 0;
        for (const FString& Line : Lines)
        {
            FString TrimmedLine = Line.TrimStartAndEnd();
            if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("#")))
                continue;
            
            // Parse plugin name (format: "PluginName Version")
            TArray<FString> Parts;
            TrimmedLine.ParseIntoArrayWS(Parts);
            if (Parts.Num() > 0)
            {
                FString PluginName = Parts[0];
                if (LoadPluginWithPath(PluginName) == 0)
                {
                    loadedCount++;
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Loaded %d plugins from plugin_list.conf"), loadedCount);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] Failed to read plugin_list.conf, falling back to basic plugins"));
        LoadPluginWithPath(TEXT("Sofa.Component"));
        LoadPluginWithPath(TEXT("Sofa.GL.Component"));
        LoadPluginWithPath(TEXT("Sofa.GUI.Component"));
    }

    // Always fetch plugin log messages to debug issues
    catchSofaMessages();
}




SofaPhysicsOutputMesh* ASofaContext::getOutputMeshByName(const FString& name)
{
    if (m_sofaAPI == nullptr || m_status <= 0)
        return nullptr;

    unsigned int nbr = m_sofaAPI->getNbOutputMeshes();
    for (unsigned int meshID = 0; meshID < nbr; meshID++)
    {
        SofaPhysicsOutputMesh* mesh = m_sofaAPI->getOutputMeshPtr(meshID);
        if (mesh)
        {
            FString meshName(mesh->getName());
            if (meshName.Equals(name, ESearchCase::IgnoreCase) || meshName.Contains(name))
            {
                UE_LOG(LogTemp, Warning, TEXT("[SOFA] Found mesh '%s' at index %d"), *meshName, meshID);
                return mesh;
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Mesh '%s' not found among %d meshes"), *name, nbr);
    return nullptr;
}

void ASofaContext::catchSofaMessages()
{
    if (m_sofaAPI == nullptr)
        return;
        
    int nbrMsgs = m_sofaAPI->getNbMessages();
    if (nbrMsgs > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Found %d messages from SOFA:"), nbrMsgs);
    }
    
    int* type = new int[1];
    type[0] = -1;
    for (int i = 0; i < nbrMsgs; ++i)
    {
        const char* rawMsg = m_sofaAPI->getMessage(i, *type);
        FString FMessage(rawMsg);

        if (type[0] == -1) {
            UE_LOG(LogTemp, Log, TEXT("[SOFA MSG] (type -1): %s"), *FMessage);
        }
        else if (type[0] == 3) {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA MSG] WARNING: %s"), *FMessage);
        }
        else if (type[0] == 4) {
            UE_LOG(LogTemp, Error, TEXT("[SOFA MSG] ERROR: %s"), *FMessage);
        }
        else if (type[0] == 5) {
            UE_LOG(LogTemp, Error, TEXT("[SOFA MSG] FATAL: %s"), *FMessage);
        }
        else {
            UE_LOG(LogTemp, Log, TEXT("[SOFA MSG] (type %d): %s"), type[0], *FMessage);
        }
    }

    m_sofaAPI->clearMessages();

    delete[] type;
}

bool ASofaContext::HasExistingVisualMeshes()
{
    // Check if there are any SofaVisualMesh actors that:
    // 1. Reference this context explicitly, OR
    // 2. Are unassigned and would auto-connect to us (only if we're the only context)
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASofaVisualMesh::StaticClass(), FoundActors);
    
    // Count how many SofaContext actors exist
    TArray<AActor*> ContextActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASofaContext::StaticClass(), ContextActors);
    bool isOnlyContext = (ContextActors.Num() <= 1);
    
    for (AActor* Actor : FoundActors)
    {
        ASofaVisualMesh* VisualMesh = Cast<ASofaVisualMesh>(Actor);
        if (VisualMesh)
        {
            // Check if it explicitly references this context
            if (VisualMesh->SofaContextRef == this)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SOFA] Found existing visual mesh '%s' referencing this context"), *VisualMesh->GetName());
                return true;
            }
            // If we're the only context and this mesh has no reference, it will auto-connect to us
            if (isOnlyContext && VisualMesh->SofaContextRef == nullptr)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SOFA] Found existing unassigned visual mesh '%s' that will auto-connect"), *VisualMesh->GetName());
                return true;
            }
        }
    }
    return false;
}

void ASofaContext::SpawnVisualMeshActors()
{
    if (m_sofaAPI == nullptr || m_status <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Cannot spawn visual meshes - scene not loaded"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA] No world available for spawning"));
        return;
    }

    unsigned int numMeshes = m_sofaAPI->getNbOutputMeshes();
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Auto-spawning %d visual mesh actors..."), numMeshes);

    for (unsigned int i = 0; i < numMeshes; i++)
    {
        SofaPhysicsOutputMesh* sofaMesh = m_sofaAPI->getOutputMeshPtr(i);
        if (!sofaMesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA] Mesh %d is null, skipping"), i);
            continue;
        }

        FString MeshNameStr(sofaMesh->getName());
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Spawning visual mesh for '%s'"), *MeshNameStr);

        // Spawn the visual mesh actor
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ASofaVisualMesh* VisualMesh = World->SpawnActor<ASofaVisualMesh>(
            ASofaVisualMesh::StaticClass(), 
            GetActorLocation(), 
            GetActorRotation(), 
            SpawnParams
        );
        
        if (VisualMesh)
        {
            // Attach to this context and set up
            VisualMesh->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
            VisualMesh->SofaContextRef = this;
            VisualMesh->MeshName = MeshNameStr;
            VisualMesh->setSofaMesh(sofaMesh);

#if WITH_EDITOR
            VisualMesh->SetActorLabel(MeshNameStr);
#endif

            UE_LOG(LogTemp, Warning, TEXT("[SOFA] Successfully spawned visual mesh '%s'"), *MeshNameStr);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[SOFA] Failed to spawn visual mesh for '%s'"), *MeshNameStr);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Finished auto-spawning visual meshes"));
}

float ASofaContext::DetectFloorHeight()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] No world available"));
        return TNumericLimits<float>::Lowest();
    }

    float DetectedFloorHeight = TNumericLimits<float>::Lowest();
    bool bFoundFloor = false;

    TArray<AActor*> StaticMeshActors;
    UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), StaticMeshActors);

    for (AActor* Actor : StaticMeshActors)
    {
        AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
        if (!StaticMeshActor)
            continue;

        UStaticMeshComponent* MeshComp = StaticMeshActor->GetStaticMeshComponent();
        if (!MeshComp || !MeshComp->GetStaticMesh())
            continue;

        FString MeshActorName = Actor->GetName().ToLower();
        
        // Check if name contains "floor" or "ground" (case insensitive)
        bool bIsFloor = MeshActorName.Contains(TEXT("floor")) || 
                        MeshActorName.Contains(TEXT("ground"));

        if (!bIsFloor)
            continue;

        FBox Bounds = MeshComp->Bounds.GetBox();
        float ActorTopZ = Bounds.Max.Z;
        
        // Calculate floor height relative to SofaContext position
        FVector ContextLocation = GetActorLocation();
        float RelativeZ = ActorTopZ - ContextLocation.Z;

        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Found floor '%s' at world Z=%f, relative Z=%f"), 
            *Actor->GetName(), ActorTopZ, RelativeZ);
        
        FVector ContextScale = GetActorScale3D();
        float SofaFloorY = RelativeZ / FMath::Abs(ContextScale.Z);
        
        if (!bFoundFloor || SofaFloorY > DetectedFloorHeight)
        {
            DetectedFloorHeight = SofaFloorY;
            bFoundFloor = true;
        }
    }

    if (bFoundFloor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Detected floor height: %f"), DetectedFloorHeight);
        return DetectedFloorHeight;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] No floor mesh found"));
        return TNumericLimits<float>::Lowest();
    }
}

FString ASofaContext::ProcessSceneForFloor(const FString& OriginalPath)
{
    if (!bEnableFloorCollision)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Floor collision disabled"));
        return OriginalPath;
    }

    // Detect floor height from Unreal scene
    float DetectedFloor = DetectFloorHeight();
    bool bFloorDetected = (DetectedFloor > TNumericLimits<float>::Lowest() + 1.0f);

    if (!bFloorDetected)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] No floor mesh found, using original scene"));
        return OriginalPath;
    }

    UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Using floor height: %f"), DetectedFloor);

    // Read the original scene file
    FString SceneContent;
    if (!FFileHelper::LoadFileToString(SceneContent, *OriginalPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA Floor] Failed to read scene file: %s"), *OriginalPath);
        return OriginalPath;
    }

    bool bModified = false;

    // Check if PlaneForceField already exists in the scene
    bool bHasPlaneForceField = SceneContent.Contains(TEXT("PlaneForceField"));

    if (bHasPlaneForceField)
    {
        // Modify existing PlaneForceField d values using simple string search
        int32 SearchStart = 0;
        while (true)
        {
            int32 PlanePos = SceneContent.Find(TEXT("<PlaneForceField"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
            if (PlanePos == INDEX_NONE)
                break;
            
            // Find the end of this tag
            int32 TagEnd = SceneContent.Find(TEXT("/>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, PlanePos);
            if (TagEnd == INDEX_NONE)
            {
                SearchStart = PlanePos + 16;
                continue;
            }
            
            // Extract the tag content
            FString TagContent = SceneContent.Mid(PlanePos, TagEnd - PlanePos + 2);
            
            // Find d=" within this tag
            int32 DAttrPos = TagContent.Find(TEXT(" d=\""), ESearchCase::IgnoreCase);
            if (DAttrPos == INDEX_NONE)
            {
                SearchStart = TagEnd + 2;
                continue;
            }
            
            // Find the closing quote of d value
            int32 DValueStart = DAttrPos + 4; // After ' d="'
            int32 DValueEnd = TagContent.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, DValueStart);
            if (DValueEnd == INDEX_NONE)
            {
                SearchStart = TagEnd + 2;
                continue;
            }
            
            // Build new tag with replaced d value
            FString NewTag = TagContent.Left(DValueStart) + 
                             FString::Printf(TEXT("%f"), DetectedFloor) + 
                             TagContent.Mid(DValueEnd);
            
            // Replace in scene content
            SceneContent = SceneContent.Left(PlanePos) + NewTag + SceneContent.Mid(TagEnd + 2);
            bModified = true;
            
            UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Modified PlaneForceField d value to %f"), DetectedFloor);
            
            SearchStart = PlanePos + NewTag.Len();
        }
    }
    else if (bFloorDetected)
    {
        // Only inject PlaneForceField if we actually detected a floor in Unreal
        // Find each Node that has a TetrahedronFEMForceField or TriangularFEMForceField (physics nodes)
        // and inject PlaneForceField into them
        
        FString PlaneForceFieldXml = FString::Printf(
            TEXT("\n        <!-- Auto-injected floor by SofaUE5 (detected from Unreal floor mesh) -->\n")
            TEXT("        <PlaneForceField normal=\"0 1 0\" d=\"%f\" stiffness=\"%f\" showPlane=\"0\"/>\n"),
            DetectedFloor, FloorStiffness
        );

        // Find nodes with FEM force fields and add floor to them
        TArray<FString> FEMPatterns = { TEXT("TetrahedronFEMForceField"), TEXT("TriangularFEMForceField") };
        
        for (const FString& Pattern : FEMPatterns)
        {
            int32 SearchStart = 0;
            while (true)
            {
                int32 FEMPos = SceneContent.Find(Pattern, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
                if (FEMPos == INDEX_NONE)
                    break;
                
                // Find the closing tag of this line (/>)
                int32 LineEnd = SceneContent.Find(TEXT("/>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FEMPos);
                if (LineEnd != INDEX_NONE)
                {
                    // Insert PlaneForceField after the FEM force field
                    SceneContent.InsertAt(LineEnd + 2, PlaneForceFieldXml);
                    bModified = true;
                    UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Injected PlaneForceField after %s at d=%f"), *Pattern, DetectedFloor);
                    SearchStart = LineEnd + 2 + PlaneForceFieldXml.Len();
                }
                else
                {
                    SearchStart = FEMPos + Pattern.Len();
                }
            }
        }
    }

    // Inject SofaCollisionMesh actors
    FString ContentWithCollisions = InjectCollisionMeshes(SceneContent);
    if (ContentWithCollisions != SceneContent)
    {
        SceneContent = ContentWithCollisions;
        bModified = true;
    }

    if (!bModified)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] No modifications made to scene"));
        return OriginalPath;
    }

    // Save to a temporary file - use absolute path to avoid SOFA path resolution issues
    // ProjectDir() is already absolute from earlier conversion
    FString TempDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Saved") / TEXT("SofaTemp");
    IFileManager::Get().MakeDirectory(*TempDir, true);
    
    FString OriginalFilename = FPaths::GetCleanFilename(OriginalPath);
    FString TempPath = TempDir / (TEXT("processed_") + OriginalFilename);
    
    // Normalize path separators for SOFA (use forward slashes)
    TempPath.ReplaceCharInline('\\', '/');
    
    UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] TempPath (absolute): %s"), *TempPath);
    
    if (FFileHelper::SaveStringToFile(SceneContent, *TempPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Saved processed scene to: %s"), *TempPath);
        return TempPath;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[SOFA Floor] Failed to save processed scene"));
        return OriginalPath;
    }
}

FString ASofaContext::InjectCollisionMeshes(const FString& SceneContent)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return SceneContent;
    }

    // Find all SofaCollisionMesh actors that reference this context
    TArray<AActor*> CollisionActors;
    UGameplayStatics::GetAllActorsOfClass(World, ASofaCollisionMesh::StaticClass(), CollisionActors);

    if (CollisionActors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Collision] No SofaCollisionMesh actors found in scene"));
        return SceneContent;
    }

    FString CollisionXML;
    int32 InjectedCount = 0;

    for (AActor* Actor : CollisionActors)
    {
        ASofaCollisionMesh* CollisionMesh = Cast<ASofaCollisionMesh>(Actor);
        if (!CollisionMesh)
            continue;

        // Check if this collision mesh references this context (or no context = use any)
        if (CollisionMesh->SofaContextRef != nullptr && CollisionMesh->SofaContextRef != this)
            continue;

        FString MeshXML = CollisionMesh->GenerateSofaSceneXML();
        if (!MeshXML.IsEmpty())
        {
            CollisionXML += MeshXML;
            InjectedCount++;
            UE_LOG(LogTemp, Warning, TEXT("[SOFA Collision] Generated collision XML for '%s'"), *CollisionMesh->GetName());
        }
    }

    if (CollisionXML.IsEmpty())
    {
        return SceneContent;
    }

    // Find the last </Node> (closing of root node) and insert before it
    FString ModifiedContent = SceneContent;
    int32 LastNodeClose = ModifiedContent.Find(TEXT("</Node>"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
    
    if (LastNodeClose != INDEX_NONE)
    {
        ModifiedContent.InsertAt(LastNodeClose, CollisionXML);
        UE_LOG(LogTemp, Warning, TEXT("[SOFA Collision] Injected %d collision meshes into scene"), InjectedCount);
    }

    return ModifiedContent;
}
