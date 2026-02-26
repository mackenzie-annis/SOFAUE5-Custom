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
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Misc/FileHelper.h"
#include "XmlFile.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
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
    SetActorScale3D(FVector(1.0, 1.0, 1.0));
    SetActorRotation(FRotator(90.0, 0.0, 0.0));

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

    // Only spawn during gameplay, not in editor (prevents WP serialization issues)
    if (!World->IsGameWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Skipping auto-spawn in editor - use Play to spawn visual meshes"));
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

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.bNoFail = true;

        ASofaVisualMesh* VisualMesh = World->SpawnActor<ASofaVisualMesh>(
            ASofaVisualMesh::StaticClass(), 
            GetActorLocation(), 
            GetActorRotation(), 
            SpawnParams
        );
        
        if (VisualMesh)
        {
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

bool ASofaContext::IsActorFloor(AActor* Actor, UStaticMeshComponent* MeshComp) const
{
    TArray<FString> Substrings = FloorActorNameSubstrings.Num() > 0
        ? FloorActorNameSubstrings
        : TArray<FString>{ TEXT("floor"), TEXT("ground") };

    FString ActorNameLower = Actor->GetName().ToLower();
    FString MeshNameLower;
    if (MeshComp && MeshComp->GetStaticMesh())
    {
        MeshNameLower = MeshComp->GetStaticMesh()->GetName().ToLower();
    }

    for (const FString& Sub : Substrings)
    {
        FString SubLower = Sub.TrimStartAndEnd().ToLower();
        if (SubLower.IsEmpty()) continue;
        if (ActorNameLower.Contains(SubLower) || MeshNameLower.Contains(SubLower))
            return true;
    }
    return false;
}

FString ASofaContext::ProcessSceneForFloor(const FString& OriginalPath)
{
    if (!bAutoDetectFloor)
        return OriginalPath;

    FString SceneContent;
    if (!FFileHelper::LoadFileToString(SceneContent, *OriginalPath))
        return OriginalPath;

    bool bModified = false;

    FString ObjPath = ExportFloorCollisionMesh();
    if (!ObjPath.IsEmpty())
    {
        FString FloorMeshXML = GenerateFloorMeshCollisionXML(ObjPath);
        int32 LastNodeClose = SceneContent.Find(TEXT("</Node>"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (LastNodeClose != INDEX_NONE)
        {
            SceneContent.InsertAt(LastNodeClose, FloorMeshXML);
            bModified = true;
            UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Injected grid-sampled floor collision"));
        }

        int32 ProxPos = SceneContent.Find(TEXT("MinProximityIntersection"));
        if (ProxPos != INDEX_NONE)
        {
            int32 TagEnd = SceneContent.Find(TEXT("/>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ProxPos);
            if (TagEnd != INDEX_NONE)
            {
                FString OldTag = SceneContent.Mid(ProxPos, TagEnd - ProxPos + 2);
                FString NewTag = TEXT("MinProximityIntersection name=\"Proximity\" alarmDistance=\"3.0\" contactDistance=\"1.0\" />");
                SceneContent = SceneContent.Replace(*OldTag, *NewTag);
            }
        }

        if (SceneContent.Contains(TEXT("<DefaultAnimationLoop")))
        {
            SceneContent = SceneContent.Replace(
                TEXT("<DefaultAnimationLoop />"),
                TEXT("<FreeMotionAnimationLoop />\n    <GenericConstraintSolver maxIterations=\"30\" tolerance=\"1e-3\" />\n    <CollisionPipeline name=\"Pipeline\" />"));
            SceneContent = SceneContent.Replace(
                TEXT("<DefaultAnimationLoop/>"),
                TEXT("<FreeMotionAnimationLoop />\n    <GenericConstraintSolver maxIterations=\"30\" tolerance=\"1e-3\" />\n    <CollisionPipeline name=\"Pipeline\" />"));
        }

        if (SceneContent.Contains(TEXT("<DefaultContactManager")))
        {
            int32 DCMPos = SceneContent.Find(TEXT("<DefaultContactManager"));
            if (DCMPos != INDEX_NONE)
            {
                int32 TagEnd = SceneContent.Find(TEXT("/>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, DCMPos);
                if (TagEnd != INDEX_NONE)
                {
                    FString OldTag = SceneContent.Mid(DCMPos, TagEnd - DCMPos + 2);
                    SceneContent = SceneContent.Replace(*OldTag, TEXT("<CollisionResponse name=\"ContactManager\" response=\"FrictionContactConstraint\" responseParams=\"mu=0.95\" />"));
                }
            }
        }

        if (SceneContent.Contains(TEXT("<MinProximityIntersection")))
        {
            int32 MPIPos = SceneContent.Find(TEXT("<MinProximityIntersection"));
            if (MPIPos != INDEX_NONE)
            {
                int32 TagEnd = SceneContent.Find(TEXT("/>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, MPIPos);
                if (TagEnd != INDEX_NONE)
                {
                    FString OldTag = SceneContent.Mid(MPIPos, TagEnd - MPIPos + 2);
                    SceneContent = SceneContent.Replace(*OldTag, TEXT("<NewProximityIntersection name=\"Intersection\" alarmDistance=\"3.0\" contactDistance=\"1.0\" />"));
                }
            }
        }

        int32 SearchPos = 0;
        while (true)
        {
            int32 FEMPos = SceneContent.Find(TEXT("TetrahedronFEMForceField"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchPos);
            if (FEMPos == INDEX_NONE)
                break;
            int32 TagEnd = SceneContent.Find(TEXT("/>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FEMPos);
            if (TagEnd == INDEX_NONE)
            {
                SearchPos = FEMPos + 20;
                continue;
            }
            int32 NextNodeEnd = SceneContent.Find(TEXT("</Node>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, TagEnd);
            if (NextNodeEnd != INDEX_NONE)
            {
                FString NodeContent = SceneContent.Mid(TagEnd, NextNodeEnd - TagEnd);
                if (!NodeContent.Contains(TEXT("UncoupledConstraintCorrection")))
                {
                    SceneContent.InsertAt(TagEnd + 2, TEXT("\n        <UncoupledConstraintCorrection />"));
                }
            }
            SearchPos = TagEnd + 50;
        }
    }

    FString ContentWithCollisions = InjectCollisionMeshes(SceneContent);
    if (ContentWithCollisions != SceneContent)
    {
        SceneContent = ContentWithCollisions;
        bModified = true;
    }

    if (!bModified)
        return OriginalPath;

    FString TempDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Saved") / TEXT("SofaTemp");
    IFileManager::Get().MakeDirectory(*TempDir, true);
    
    FString OriginalFilename = FPaths::GetCleanFilename(OriginalPath);
    FString TempPath = TempDir / (TEXT("processed_") + OriginalFilename);
    TempPath.ReplaceCharInline('\\', '/');
    
    if (FFileHelper::SaveStringToFile(SceneContent, *TempPath))
        return TempPath;
    
    return OriginalPath;
}

void ASofaContext::ClearSceneCache()
{
    FString TempDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Saved") / TEXT("SofaTemp");
    IFileManager& FileMan = IFileManager::Get();

    if (!filePath.FilePath.IsEmpty())
    {
        FString ProcessedName = TEXT("processed_") + FPaths::GetCleanFilename(filePath.FilePath);
        FString ProcessedPath = TempDir / ProcessedName;
        ProcessedPath.ReplaceCharInline('\\', '/');
        if (FileMan.Delete(*ProcessedPath))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA] Cleared processed scene cache: %s"), *ProcessedPath);
        }
        else if (FileMan.FileExists(*ProcessedPath))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SOFA] Failed to delete processed scene: %s"), *ProcessedPath);
        }
    }

    FString ObjPath = TempDir / TEXT("floor_collision.obj");
    ObjPath.ReplaceCharInline('\\', '/');
    if (FileMan.Delete(*ObjPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] Cleared floor collision cache: %s"), *ObjPath);
    }

    CachedFloorObjPath.Empty();
    CachedFloorLocation = FVector::ZeroVector;
    UE_LOG(LogTemp, Warning, TEXT("[SOFA] Scene cache cleared. Next Play will regenerate processed scene and floor."));
}

FString ASofaContext::InjectCollisionMeshes(const FString& SceneContent)
{
    // TODO: Implement SofaCollisionMesh actor support for custom collision objects
    return SceneContent;
}

FString ASofaContext::ExportFloorCollisionMesh()
{
    UWorld* World = GetWorld();
    if (!World)
        return FString();

    FVector ContextLocation = GetActorLocation();
    FVector ContextScale = GetActorScale3D();
    
    FString TempDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("Saved") / TEXT("SofaTemp");
    FString ObjPath = TempDir / TEXT("floor_collision.obj");
    ObjPath.ReplaceCharInline('\\', '/');
    
    bool bLocationChanged = FVector::DistSquared(ContextLocation, CachedFloorLocation) > 100.0f;
    if (!CachedFloorObjPath.IsEmpty() && !bLocationChanged && FPaths::FileExists(ObjPath))
    {
        UE_LOG(LogTemp, Log, TEXT("[SOFA] Using cached floor collision mesh"));
        return CachedFloorObjPath;
    }

    TArray<FVector> GridVertices;
    TArray<int32> GridIndices;
    
    int32 GridResolution = FMath::Clamp(FloorGridDivisions, 2, 50);
    float HalfRadius = FloorGridSize * 0.5f;
    float CellSize = FloorGridSize / GridResolution;
    
    TArray<TArray<float>> HeightGrid;
    HeightGrid.SetNum(GridResolution + 1);
    for (int32 i = 0; i <= GridResolution; i++)
        HeightGrid[i].SetNum(GridResolution + 1);
    
    TArray<TArray<bool>> ValidGrid;
    ValidGrid.SetNum(GridResolution + 1);
    for (int32 i = 0; i <= GridResolution; i++)
    {
        ValidGrid[i].SetNum(GridResolution + 1);
        for (int32 j = 0; j <= GridResolution; j++)
            ValidGrid[i][j] = false;
    }
    
    TArray<AActor*> LandscapeActors;
    UGameplayStatics::GetAllActorsOfClass(World, ALandscapeProxy::StaticClass(), LandscapeActors);
    UE_LOG(LogTemp, Warning, TEXT("[SOFA Floor] Found %d landscape actors"), LandscapeActors.Num());
    
    // Use only the landscape that contains the context (avoids mixing heights from multiple proxies / streaming).
    // Heights come from landscape collision; for lumpy terrain to match the visible ground, set the landscape
    // Collision Mip Level to 0 (Details > Collision) so collision uses the full-resolution heightmap.
    ALandscapeProxy* PrimaryLandscape = nullptr;
    FBox ContextBounds(ContextLocation - FVector(HalfRadius, HalfRadius, 0), ContextLocation + FVector(HalfRadius, HalfRadius, 0));
    for (AActor* Actor : LandscapeActors)
    {
        ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Actor);
        if (!Landscape)
            continue;
        FBox LandscapeBounds = Landscape->GetComponentsBoundingBox();
        bool bOverlap = (LandscapeBounds.Min.X <= ContextBounds.Max.X && LandscapeBounds.Max.X >= ContextBounds.Min.X &&
                         LandscapeBounds.Min.Y <= ContextBounds.Max.Y && LandscapeBounds.Max.Y >= ContextBounds.Min.Y);
        if (bOverlap)
        {
            PrimaryLandscape = Landscape;
            break;
        }
    }
    if (!PrimaryLandscape && LandscapeActors.Num() > 0)
        PrimaryLandscape = Cast<ALandscapeProxy>(LandscapeActors[0]);
    
    if (PrimaryLandscape)
    {
        for (int32 GridY = 0; GridY <= GridResolution; GridY++)
        {
            for (int32 GridX = 0; GridX <= GridResolution; GridX++)
            {
                float WorldX = ContextLocation.X - HalfRadius + (GridX * CellSize);
                float WorldY = ContextLocation.Y - HalfRadius + (GridY * CellSize);
                
                FVector Start(WorldX, WorldY, ContextLocation.Z + 10000.0f);
                FVector End(WorldX, WorldY, ContextLocation.Z - 10000.0f);
                
                FHitResult Hit;
                if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic))
                {
                    if (Hit.GetActor() == PrimaryLandscape)
                    {
                        HeightGrid[GridY][GridX] = Hit.Location.Z;
                        ValidGrid[GridY][GridX] = true;
                    }
                }
            }
        }
        UE_LOG(LogTemp, Log, TEXT("[SOFA] Sampled landscape '%s' (heightmap from collision)"), *PrimaryLandscape->GetName());
    }
    
    TArray<AActor*> StaticMeshActors;
    UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), StaticMeshActors);
    
    for (AActor* Actor : StaticMeshActors)
    {
        UStaticMeshComponent* MeshComp = Cast<AStaticMeshActor>(Actor) ? Cast<AStaticMeshActor>(Actor)->GetStaticMeshComponent() : nullptr;
        if (!IsActorFloor(Actor, MeshComp))
            continue;
            
        for (int32 GridY = 0; GridY <= GridResolution; GridY++)
        {
            for (int32 GridX = 0; GridX <= GridResolution; GridX++)
            {
                if (ValidGrid[GridY][GridX])
                    continue;
                    
                float WorldX = ContextLocation.X - HalfRadius + (GridX * CellSize);
                float WorldY = ContextLocation.Y - HalfRadius + (GridY * CellSize);
                
                FVector Start(WorldX, WorldY, ContextLocation.Z + 10000.0f);
                FVector End(WorldX, WorldY, ContextLocation.Z - 10000.0f);
                
                FHitResult Hit;
                if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic))
                {
                    if (Hit.GetActor() == Actor)
                    {
                        HeightGrid[GridY][GridX] = Hit.Location.Z;
                        ValidGrid[GridY][GridX] = true;
                    }
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("[SOFA] Sampled floor mesh '%s'"), *Actor->GetName());
    }
    
    for (int32 GridY = 0; GridY <= GridResolution; GridY++)
    {
        for (int32 GridX = 0; GridX <= GridResolution; GridX++)
        {
            if (!ValidGrid[GridY][GridX])
            {
                float Sum = 0.0f;
                int32 Count = 0;
                for (int32 dy = -1; dy <= 1; dy++)
                {
                    for (int32 dx = -1; dx <= 1; dx++)
                    {
                        int32 nx = GridX + dx;
                        int32 ny = GridY + dy;
                        if (nx >= 0 && nx <= GridResolution && ny >= 0 && ny <= GridResolution && ValidGrid[ny][nx])
                        {
                            Sum += HeightGrid[ny][nx];
                            Count++;
                        }
                    }
                }
                if (Count > 0)
                {
                    HeightGrid[GridY][GridX] = Sum / Count;
                    ValidGrid[GridY][GridX] = true;
                }
            }
        }
    }
    
    TMap<TPair<int32, int32>, int32> GridToVertexIndex;
    
    for (int32 GridY = 0; GridY <= GridResolution; GridY++)
    {
        for (int32 GridX = 0; GridX <= GridResolution; GridX++)
        {
            if (ValidGrid[GridY][GridX])
            {
                float WorldX = ContextLocation.X - HalfRadius + (GridX * CellSize);
                float WorldY = ContextLocation.Y - HalfRadius + (GridY * CellSize);
                float WorldZ = HeightGrid[GridY][GridX];
                
                GridToVertexIndex.Add(TPair<int32, int32>(GridX, GridY), GridVertices.Num());
                GridVertices.Add(FVector(WorldX, WorldY, WorldZ));
            }
        }
    }
    
    for (int32 GridY = 0; GridY < GridResolution; GridY++)
    {
        for (int32 GridX = 0; GridX < GridResolution; GridX++)
        {
            bool bHas00 = ValidGrid[GridY][GridX];
            bool bHas10 = ValidGrid[GridY][GridX + 1];
            bool bHas01 = ValidGrid[GridY + 1][GridX];
            bool bHas11 = ValidGrid[GridY + 1][GridX + 1];
            
            if (bHas00 && bHas10 && bHas01)
            {
                GridIndices.Add(GridToVertexIndex[TPair<int32, int32>(GridX, GridY)]);
                GridIndices.Add(GridToVertexIndex[TPair<int32, int32>(GridX + 1, GridY)]);
                GridIndices.Add(GridToVertexIndex[TPair<int32, int32>(GridX, GridY + 1)]);
            }
            
            if (bHas10 && bHas11 && bHas01)
            {
                GridIndices.Add(GridToVertexIndex[TPair<int32, int32>(GridX + 1, GridY)]);
                GridIndices.Add(GridToVertexIndex[TPair<int32, int32>(GridX + 1, GridY + 1)]);
                GridIndices.Add(GridToVertexIndex[TPair<int32, int32>(GridX, GridY + 1)]);
            }
        }
    }
    
    if (GridVertices.Num() < 3 || GridIndices.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SOFA] No floor geometry found within radius"));
        return FString();
    }
    
    FString ObjContent;
    for (const FVector& Vertex : GridVertices)
    {
        FVector RelativePos = Vertex - ContextLocation;
        float SofaX = RelativePos.X / FMath::Abs(ContextScale.X);
        float SofaY = RelativePos.Z / FMath::Abs(ContextScale.Z);
        float SofaZ = -RelativePos.Y / FMath::Abs(ContextScale.Y);
        ObjContent += FString::Printf(TEXT("v %f %f %f\n"), SofaX, SofaY, SofaZ);
    }
    
    for (int32 i = 0; i < GridIndices.Num(); i += 3)
    {
        ObjContent += FString::Printf(TEXT("f %d %d %d\n"), 
            GridIndices[i] + 1, GridIndices[i + 1] + 1, GridIndices[i + 2] + 1);
    }
    
    IFileManager::Get().MakeDirectory(*TempDir, true);
    
    if (FFileHelper::SaveStringToFile(ObjContent, *ObjPath))
    {
        CachedFloorObjPath = ObjPath;
        CachedFloorLocation = ContextLocation;
        UE_LOG(LogTemp, Log, TEXT("[SOFA] Exported floor: %d verts, %d tris"), 
            GridVertices.Num(), GridIndices.Num() / 3);
        return ObjPath;
    }
    
    return FString();
}

FString ASofaContext::GenerateFloorMeshCollisionXML(const FString& ObjFilePath)
{
    if (ObjFilePath.IsEmpty())
        return FString();

    FString XML = TEXT("\n    <Node name=\"FloorCollision\">\n");
    XML += FString::Printf(TEXT("        <MeshOBJLoader name=\"floorLoader\" filename=\"%s\" />\n"), *ObjFilePath);
    XML += TEXT("        <MeshTopology src=\"@floorLoader\" />\n");
    XML += TEXT("        <MechanicalObject src=\"@floorLoader\" name=\"FloorCollisModel\" />\n");
    XML += TEXT("        <TriangleCollisionModel simulated=\"false\" moving=\"false\" contactStiffness=\"800\" />\n");
    XML += TEXT("        <OglModel name=\"FloorVisual\" src=\"@floorLoader\" color=\"0.3 0.6 0.3 0.5\" />\n");
    XML += TEXT("    </Node>\n");
    return XML;
}