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
#include "SofaVisualMesh.h"
#include "SofaUE5.h"
#include "SofaContext.h"
#include "SofaUE5Library/SofaPhysicsAPI.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ASofaVisualMesh::ASofaVisualMesh()
    : m_isStatic(false)
    , m_min(FVector(100000, 100000, 100000))
    , m_max(FVector(-100000, -100000, -100000))
{
    UE_LOG(SUnreal_log, Warning, TEXT("##### ASofaVisualMesh::ASofaVisualMesh() ####"));
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = mesh;
}

void ASofaVisualMesh::setSofaMesh(SofaPhysicsOutputMesh* sofaMesh)
{
    m_sofaMesh = sofaMesh;
    createMesh();
}

void ASofaVisualMesh::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] SofaVisualMesh::BeginPlay() called for '%s'"), *GetName());
    
    // Clear the mesh pointer so we reconnect to the new API
    if (m_sofaMesh != nullptr)
    {
        UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] Clearing stale mesh pointer for '%s' - will reconnect to new API"), *MeshName);
        m_sofaMesh = nullptr;
    }
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ASofaVisualMesh::PostActorCreated()
{
    Super::PostActorCreated();
}

// This is called when actor is already in level and map is opened
void ASofaVisualMesh::PostLoad()
{
    Super::PostLoad();
}

void ASofaVisualMesh::Tick( float DeltaTime )
{
    Super::Tick( DeltaTime );

    // Try to connect to SOFA mesh if not connected yet
    if (m_sofaMesh == nullptr)
    {
        // Auto-detect SofaContext if not set
        if (!SofaContextRef)
        {
            // First try immediate parent
            AActor* Parent = GetAttachParentActor();
            if (Parent)
            {
                SofaContextRef = Cast<ASofaContext>(Parent);
            }
            
            // If not found, search for any SofaContext in the level
            if (!SofaContextRef)
            {
                TArray<AActor*> FoundActors;
                UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASofaContext::StaticClass(), FoundActors);
                if (FoundActors.Num() > 0)
                {
                    SofaContextRef = Cast<ASofaContext>(FoundActors[0]);
                    UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] SofaVisualMesh: Auto-detected SofaContext '%s' in level"), *SofaContextRef->GetName());
                }
            }
        }

        if (SofaContextRef)
        {
            if (SofaContextRef->isSceneLoaded())
            {
                // Use actor name as mesh name if MeshName is empty
                FString searchName = MeshName.IsEmpty() ? GetName() : MeshName;
                UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] SofaVisualMesh::Tick - Looking for mesh '%s'"), *searchName);
                
                SofaPhysicsOutputMesh* sofaMesh = SofaContextRef->getOutputMeshByName(searchName);
                if (sofaMesh)
                {
                    UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] SofaVisualMesh: Found mesh '%s', creating visual"), *searchName);
                    setSofaMesh(sofaMesh);
                }
                else
                {
                    UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] SofaVisualMesh: Mesh '%s' not found in SOFA scene"), *searchName);
                }
            }
        }
    }

    if (!m_isStatic && m_sofaMesh != nullptr)
        updateMesh();
}


void ASofaVisualMesh::updateMesh()
{   
    if (m_sofaMesh == nullptr)
        return;

    int nbrV = m_sofaMesh->getNbVertices();
    if (nbrV <= 0)
        return;

    float* sofaVertices = new float[nbrV * 3];
    float* sofaNormals = new float[nbrV * 3];
    m_sofaMesh->getVPositions(sofaVertices);
    m_sofaMesh->getVNormals(sofaNormals);

    TArray<FVector> vertices;
    TArray<FVector> normals;
    vertices.Reserve(nbrV);
    normals.Reserve(nbrV);

    if (m_inverseNormal)
    {
        for (int i = 0; i < nbrV; i++)
        {
            vertices.Add(FVector(sofaVertices[i * 3], sofaVertices[i * 3 + 1], sofaVertices[i * 3 + 2]));
            normals.Add(FVector(-sofaNormals[i * 3], -sofaNormals[i * 3 + 1], -sofaNormals[i * 3 + 2]));
        }
    }
    else
    {
        for (int i = 0; i < nbrV; i++)
        {
            vertices.Add(FVector(sofaVertices[i * 3], sofaVertices[i * 3 + 1], sofaVertices[i * 3 + 2]));
            normals.Add(FVector(sofaNormals[i * 3], sofaNormals[i * 3 + 1], sofaNormals[i * 3 + 2]));
        }
    }

    delete[] sofaVertices;
    delete[] sofaNormals;

    mesh->UpdateMeshSection(0, vertices, normals, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
}


void ASofaVisualMesh::createMesh()
{
    if (m_sofaMesh == nullptr)
        return;

    int nbrV = m_sofaMesh->getNbVertices();
    int nbrTri = m_sofaMesh->getNbTriangles();
    int nbrQuad = m_sofaMesh->getNbQuads();
    UE_LOG(SUnreal_log, Warning, TEXT("##### ASofaVisualMesh::createMesh:  nbrV: %d | nbrTri: %d | nbrQuad: %d ####"), nbrV, nbrTri, nbrQuad);

    if (nbrV <= 0)
    {
        UE_LOG(SUnreal_log, Error, TEXT("[SOFA] ASofaVisualMesh::createMesh - No vertices!"));
        return;
    }

    // Get all info from SofaPhysicsOutputMesh
    float* sofaVertices = new float[nbrV * 3];
    float* sofaNormals = new float[nbrV * 3];
    float* sofaTexCoords = new float[nbrV * 2];
    int* sofaTriangles = new int[nbrTri * 3];
    int* sofaQuads = new int[nbrQuad * 4];

    m_sofaMesh->getVPositions(sofaVertices);
    m_sofaMesh->getVNormals(sofaNormals);
    m_sofaMesh->getVTexCoords(sofaTexCoords);
    m_sofaMesh->getTriangles(sofaTriangles);
    m_sofaMesh->getQuads(sofaQuads);

    // Create Arrays to fill UE buffers
    TArray<FVector> vertices;
    TArray<FVector> normals;
    TArray<FVector2D> UV0;
    TArray<int32> Triangles;
    TArray<FLinearColor> vertexColors;
    TArray<FProcMeshTangent> tangents;

    // Fill Unreal Mesh info with SOFA buffers
    if (m_inverseNormal)
    {
        for (int i = 0; i < nbrV; i++)
        {
            vertices.Add(FVector(sofaVertices[i * 3], sofaVertices[i * 3 + 1], sofaVertices[i * 3 + 2]));
            normals.Add(FVector(-sofaNormals[i * 3], -sofaNormals[i * 3 + 1], -sofaNormals[i * 3 + 2]));
            UV0.Add(FVector2D(sofaTexCoords[i * 2], sofaTexCoords[i * 2 + 1]));
        }
    }
    else
    {
        for (int i = 0; i < nbrV; i++)
        {
            vertices.Add(FVector(sofaVertices[i * 3], sofaVertices[i * 3 + 1], sofaVertices[i * 3 + 2]));
            normals.Add(FVector(sofaNormals[i * 3], sofaNormals[i * 3 + 1], sofaNormals[i * 3 + 2]));
            UV0.Add(FVector2D(sofaTexCoords[i * 2], sofaTexCoords[i * 2 + 1]));
        }
    }

    // Add triangles
    for (int i = 0; i < nbrTri; i++)
    {
        Triangles.Add(sofaTriangles[i * 3]);
        Triangles.Add(sofaTriangles[i * 3 + 1]);
        Triangles.Add(sofaTriangles[i * 3 + 2]);
    }

    // Add quads as 2 triangles
    for (int i = 0; i < nbrQuad; i++)
    {
        Triangles.Add(sofaQuads[i * 4]);
        Triangles.Add(sofaQuads[i * 4 + 1]);
        Triangles.Add(sofaQuads[i * 4 + 2]);

        Triangles.Add(sofaQuads[i * 4]);
        Triangles.Add(sofaQuads[i * 4 + 2]);
        Triangles.Add(sofaQuads[i * 4 + 3]);
    }

    // Recompute UV if not provided
    bool needUVRecompute = true;
    for (int i = 0; i < nbrV * 2; i++)
    {
        if (sofaTexCoords[i] != 0.0f)
        {
            needUVRecompute = false;
            break;
        }
    }

    if (needUVRecompute)
    {
        computeBoundingBox(vertices);
        recomputeUV(vertices, UV0);
    }

    // Clean up SOFA buffers
    delete[] sofaVertices;
    delete[] sofaNormals;
    delete[] sofaTexCoords;
    delete[] sofaTriangles;
    delete[] sofaQuads;

    // Create the mesh section
    mesh->CreateMeshSection_LinearColor(0, vertices, Triangles, normals, UV0, vertexColors, tangents, true);

    UE_LOG(SUnreal_log, Warning, TEXT("[SOFA] ASofaVisualMesh::createMesh - Created mesh with %d vertices, %d triangles"), vertices.Num(), Triangles.Num() / 3);
}


void ASofaVisualMesh::computeBoundingBox(const TArray<FVector>& vertices)
{
    for (const FVector& v : vertices)
    {
        m_min.X = FMath::Min(m_min.X, v.X);
        m_min.Y = FMath::Min(m_min.Y, v.Y);
        m_min.Z = FMath::Min(m_min.Z, v.Z);

        m_max.X = FMath::Max(m_max.X, v.X);
        m_max.Y = FMath::Max(m_max.Y, v.Y);
        m_max.Z = FMath::Max(m_max.Z, v.Z);
    }
}


void ASofaVisualMesh::recomputeUV(const TArray<FVector>& vertices, TArray<FVector2D>& UV0)
{
    FVector range = m_max - m_min;
    
    // Avoid division by zero
    if (range.X == 0) range.X = 1;
    if (range.Y == 0) range.Y = 1;
    if (range.Z == 0) range.Z = 1;

    UV0.Empty();
    for (const FVector& v : vertices)
    {
        // Simple planar UV mapping based on XY coordinates
        float u = (v.X - m_min.X) / range.X;
        float vCoord = (v.Y - m_min.Y) / range.Y;
        UV0.Add(FVector2D(u, vCoord));
    }
}
