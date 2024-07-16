// Fill out your copyright notice in the Description page of Project Settings.

#include "AssetConstructor.h"

#include "ImageUtils.h"
#include "LogAssetConstructor.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "ProceduralMeshConversion.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <type_traits>

#pragma region forward declarations of static functions
/**
 * Load Ai(Assimp) Scene
 * @param AiImporter Assimp Importer
 * @param FilePath Path to the file
 * @return a valid pointer in case of success, nullptr in case of failure.
 */
static const aiScene* LoadAiScene(Assimp::Importer& AiImporter,
                                  const FString&    FilePath);

/**
 * Transform the coordinate system of an assimp scene to the UE coordinate
 * system.
 * Directly overwrite mTransformation of the root node.
 * @param AiScene Ai(Assimp) Scene
 */
static void TransformToUECoordinateSystem(const aiScene& AiScene);

/**
 * Get UnitScaleFactor meta data from the scene
 * @param AiScene Ai(Assimp) Scene
 * @return the value if data is available, otherwise 1.0f
 */
static float GetAiUnitScaleFactor(const aiScene& AiScene);

/**
 * Generate a transformation matrix to transform from the Ai(Assimp) coordinate
 * system to the UE coordinate system.
 * @param AiScene Ai(Assimp) Scene
 */
static aiMatrix4x4t<float> GenerateAi_UE_XformMatrix(const aiScene& AiScene);

/**
 * Generate material instances from Ai(Assimp) Scene object.
 * @param Owner Owner of the material instances
 * @param AiScene Ai(Assimp) Scene
 * @param ParentMaterialInterface Parent MaterialInterface from which
 *                                the material instance was created
 * @return array of the material instances
 */
static TArray<UMaterialInstanceDynamic*>
    GenerateMaterialInstances(AActor& Owner, const aiScene& AiScene,
                              UMaterialInterface& ParentMaterialInterface);

/**
 * Generate material datas from Ai(Assimp) Scene object.
 * @param AiScene Ai(Assimp) Scene
 */
static TArray<FLoadedMaterialData>
    GenerateMaterialDatas(const aiScene& AiScene);

/**
 * Generate material instances from array of material datas.
 * @param Owner Owner of the material instances
 * @param MaterialDatas array of material datas
 * @param ParentMaterialInterface Parent MaterialInterface from which
 *                                the material instance was created
 * @return array of the material instances
 */
static TArray<UMaterialInstanceDynamic*>
    GenerateMaterialInstances(AActor&                            Owner,
                              const TArray<FLoadedMaterialData>& MaterialDatas,
                              UMaterialInterface& ParentMaterialInterface);

/**
 * Verify the specified material has the specified parameter.
 * Unreal "verifyf" macro is used for verifying.
 * @param   MaterialInterface       material interface to check for the presence
 *                                  of parameter
 * @param   MaterialParameterType   type of parameter to be verified
 * @param   ParameterName           name of parameter to be verified
 */
static void
    VerifyMaterialParameter(const UMaterialInterface&     MaterialInterface,
                            const EMaterialParameterType& MaterialParameterType,
                            const FName&                  ParameterName);

/**
 * Construct ProceduralMeshComponent tree recursively from the AiNode
 * @param   AiScene             assimp's scene object.
 * @param   AiNode              assimp's node object to start treeing.
 * @param   MaterialInstances   Material Instances. You need to create material
 *                              from AiScene->mMaterials and pass it to this
 *                              argument.
 * @param   Owner               Owner of the returned procedural mesh
 *                              component and its descendant.
 * @param   ShouldReplicate     Whether the component should be replicated or
 *                              not.
 * @param   ShouldRegisterComponentToOwner    Whether to register components to
 *                                            Owner. Must be turned ON to be
 *                                            reflected in the scene.
 */
static UProceduralMeshComponent* ConstructProceduralMeshComponentTree(
    const aiScene& AiScene, const aiNode& AiNode,
    const TArray<UMaterialInstanceDynamic*>& MaterialInstances, AActor& Owner,
    bool ShouldReplicate, bool ShouldRegisterComponentToOwner);

/**
 * Convert assimp's matrix to UE's matrix
 * Return transpose of the assimp's matrix as the UE's matrix. (since one is
 * transpose of the other one).
 * @param   AiMatrix4x4   assimp's matrix
 * @return  UE's matrix
 */
static FMatrix AiMatrixToUEMatrix(const aiMatrix4x4& AiMatrix4x4);

/**
 * Construct StaticMeshComponent tree recursively from the
 * ProceduralMeshComponentNode
 * @param   ProceduralMeshComponentNode   Procedural Mesh Component
 *                                        object to start treeing.
 * @param   Owner               Owner of the returned static mesh
 *                              component and its descendant.
 * @param   ShouldReplicate     Whether the component should be replicated or
 *                              not.
 * @param   ShouldRegisterComponentToOwner    Whether to register components to
 *                                            Owner. Must be turned ON to be
 *                                            reflected in the scene.
 */
static UStaticMeshComponent* ConstructStaticMeshComponentTree(
    UProceduralMeshComponent& ProceduralMeshComponentNode, AActor& Owner,
    bool ShouldReplicate, bool ShouldRegisterComponentToOwner);

/**
 * Construct DynamicMeshComponent tree recursively from the
 * ProceduralMeshComponentNode
 * @param   ProceduralMeshComponentNode   Procedural Mesh Component
 *                                        object to start treeing.
 * @param   Owner               Owner of the returned dynamic mesh
 *                              component and its descendant.
 * @param   ShouldReplicate     Whether the component should be replicated or
 *                              not.
 * @param   ShouldRegisterComponentToOwner    Whether to register components to
 *                                            Owner. Must be turned ON to be
 *                                            reflected in the scene.
 */
static UDynamicMeshComponent* ConstructDynamicMeshComponentTree(
    UProceduralMeshComponent& ProceduralMeshComponentNode, AActor& Owner,
    bool ShouldReplicate, bool ShouldRegisterComponentToOwner);
#pragma endregion

template <typename MeshComponentT>
MeshComponentT* ConstructMeshComponentFromMeshData(
    const FLoadedMeshData& MeshData,
    UMaterialInterface* ParentMaterialInterface, AActor* Owner,
    bool ShouldRegisterComponentToOwner) {
	// type MeshComponentT check
	static_assert(std::is_same_v<MeshComponentT, UProceduralMeshComponent> ||
	                  std::is_same_v<MeshComponentT, UStaticMeshComponent>,
	              "Only UProceduralMeshComponent or UStaticMeshComponent is "
	              "supported for MeshComponentT.");

	// check that the NodeList in MeshData has at least one node (because there
	// must be a root node)
	check(!MeshData.NodeList.IsEmpty());

	// check to Owner is properly set
	check(Owner != nullptr);

	// get node list
	const auto& NodeList = MeshData.NodeList;

	// number of the NodeList
	const auto& NumNodeList = NodeList.Num();

	// list of mesh components to be made
	TArray<MeshComponentT*> MeshComponentList;
	MeshComponentList.AddUninitialized(NumNodeList);

	// get material datas
	const auto& MaterialDatas = MeshData.MaterialList;

	// generate material instances
	const auto& MaterialInstances = GenerateMaterialInstances(
	    *Owner, MaterialDatas, *ParentMaterialInterface);

	// construct Mesh Component Tree
	for (auto Node_i = decltype(NumNodeList){0}; Node_i < NumNodeList; ++Node_i) {
		// get reference of the node
		const auto& Node = NodeList[Node_i];

		// new MeshComponent
		const auto& MeshComponent = NewObject<MeshComponentT>(Owner);

		// set RelativeTransform
		MeshComponent->SetRelativeTransform(Node.RelativeTransform);

		// get reference of the sections
		const auto& Sections = Node.Sections;

		// get number of sections
		const auto& NumSections = Sections.Num();

		// create mesh sections
		for (auto Section_i = decltype(NumSections){0}; Section_i < NumSections;
		     ++Section_i) {
			if constexpr (std::is_same_v<UProceduralMeshComponent, MeshComponentT>) {
				// get reference of the section
				const auto& Section = Sections[Section_i];

				// CreateCollision parameter
				constexpr auto CreateCollision = true;

				// bSRGBCConversion parameter
				constexpr auto bSRGBCConversion = false;

				// create mesh section
				MeshComponent->CreateMeshSection_LinearColor(
				    Section_i, Section.Vertices, Section.Triangles, Section.Normals,
				    Section.UV0Channel, Section.VertexColors0, Section.Tangents,
				    CreateCollision, bSRGBCConversion);

				// set Material
				const auto& MaterialIndex    = Section.MaterialIndex;
				const auto& MaterialInstance = MaterialInstances[MaterialIndex];
				MeshComponent->SetMaterial(Section_i, MaterialInstance);
			} else {
				// create transient Procedural Mesh Component
				const auto& ProceduralMeshComponent =
				    NewObject<UProceduralMeshComponent>();
				{
					// set RelativeTransform
					ProceduralMeshComponent->SetRelativeTransform(Node.RelativeTransform);

					// get reference of the section
					const auto& Section = Sections[Section_i];

					// CreateCollision parameter
					constexpr auto CreateCollision = true;

					// bSRGBCConversion parameter
					constexpr auto bSRGBCConversion = false;

					// create mesh section
					ProceduralMeshComponent->CreateMeshSection_LinearColor(
					    Section_i, Section.Vertices, Section.Triangles, Section.Normals,
					    Section.UV0Channel, Section.VertexColors0, Section.Tangents,
					    CreateCollision, bSRGBCConversion);

					// set Material
					const auto& MaterialIndex    = Section.MaterialIndex;
					const auto& MaterialInstance = MaterialInstances[MaterialIndex];
					ProceduralMeshComponent->SetMaterial(Section_i, MaterialInstance);
				}

				if constexpr (std::is_same_v<UStaticMeshComponent, MeshComponentT>) {
					// new StaticMesh
					const auto& StaticMesh      = NewObject<UStaticMesh>(Owner);
					StaticMesh->bAllowCPUAccess = true;
					StaticMesh->NeverStream     = true;
					StaticMesh->InitResources();
					StaticMesh->SetLightingGuid();

					// copy meshes
					{
						// get description of ProceduralMesh
						auto ProceduralMeshDescription =
						    BuildMeshDescription(ProceduralMeshComponent);

						// create parameters of BuildFromMeshDescriptions function
						UStaticMesh::FBuildMeshDescriptionsParams
						    BuildMeshDescriptionsParams;
#if !WITH_EDITOR
						BuildMeshDescriptionsParams.bFastBuild =
						    true; // set fast build (mandatory in non-editor builds)
						BuildMeshDescriptionsParams.bAllowCpuAccess = true;
#endif

						// copy meshes from ProceduralMeshComponent
						StaticMesh->BuildFromMeshDescriptions({&ProceduralMeshDescription},
						                                      BuildMeshDescriptionsParams);
					}

					// copy collisions
					StaticMesh->CalculateExtendedBounds();
					StaticMesh->SetBodySetup(ProceduralMeshComponent->ProcMeshBodySetup);

					// copy materials
					for (const auto& MaterialInterface :
					     ProceduralMeshComponent->GetMaterials()) {
						StaticMesh->AddMaterial(MaterialInterface);
					}

#if WITH_EDITOR
					StaticMesh->PostEditChange();
#endif

					StaticMesh->MarkPackageDirty();

					// set static mesh
					MeshComponent->SetStaticMesh(StaticMesh);
				}
			}
		}

		// get parent node index
		const auto& ParentNodeIndex = Node.ParentNodeIndex;

		// if creating a root node
		if (0 == Node_i) {
			if (ShouldRegisterComponentToOwner) {
				// register root to owning actor (Owner) to reflect in the unreal's
				// scene
				MeshComponent->RegisterComponent();
			}
		}
		// if creating a non-root node
		else {
			// get parent Mesh Component
			const auto& ParentMeshComp = MeshComponentList[ParentNodeIndex];

			// if ShouldRegisterComponentToOwner is ON
			if (ShouldRegisterComponentToOwner) {
				// Setup of attachment of this component to parent component
				MeshComponent->SetupAttachment(ParentMeshComp);

				// register component to owning actor (Owner) to reflect in the unreal's
				// scene
				MeshComponent->RegisterComponent();
			}
			// if ShouldRegisterComponentToOwner is OFF
			else {
				// attach this component to parent component
				MeshComponent->AttachToComponent(
				    ParentMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}

		// set created Mesh Component
		MeshComponentList[Node_i] = MeshComponent;
	}

	// return root MeshComponent of MeshComponentTree
	return MeshComponentList[0];
}

UProceduralMeshComponent*
    UAssetConstructor::ConstructProceduralMeshComponentFromMeshData(
        const FLoadedMeshData& MeshData,
        UMaterialInterface* ParentMaterialInterface, AActor* Owner,
        bool ShouldRegisterComponentToOwner) {
	return ConstructMeshComponentFromMeshData<UProceduralMeshComponent>(
	    MeshData, ParentMaterialInterface, Owner, ShouldRegisterComponentToOwner);
}

UStaticMeshComponent*
    UAssetConstructor::ConstructStaticMeshComponentFromMeshData(
        const FLoadedMeshData& MeshData,
        UMaterialInterface* ParentMaterialInterface, AActor* Owner,
        bool ShouldRegisterComponentToOwner) {
	return ConstructMeshComponentFromMeshData<UStaticMeshComponent>(
	    MeshData, ParentMaterialInterface, Owner, ShouldRegisterComponentToOwner);
}

UProceduralMeshComponent*
    UAssetConstructor::ConstructProceduralMeshComponentFromAssetFile(
        const FString&            FilePath,
        UMaterialInterface* const ParentMaterialInterface, AActor* const Owner,
        EConstructProceduralMeshComponentFromAssetFileResult&
                   ConstructProceduralMeshComponentFromAssetFileResult,
        const bool ShouldReplicate, const bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// construct Ai(Assimp) Importer
	Assimp::Importer AiImporter;

	// load AiScene
	const auto& AiScene = LoadAiScene(AiImporter, FilePath);

	// When a scene fails to load
	if (nullptr == AiScene) {
		// assume the result is failure
		ConstructProceduralMeshComponentFromAssetFileResult =
		    EConstructProceduralMeshComponentFromAssetFileResult::Failure;

		// return empty pointer
		return nullptr;
	}

	// Transform the coordinate system of Ai(Assimp) Scene to the UE coordinate
	// system.
	TransformToUECoordinateSystem(*AiScene);

	// make a list of materials in advance
	TArray<UMaterialInstanceDynamic*> MaterialInstances =
	    GenerateMaterialInstances(*Owner, *AiScene, *ParentMaterialInterface);

	// construct Procedural Mesh Component Tree from Root Node
	const auto& ProceduralMeshComponentTreeRoot =
	    ConstructProceduralMeshComponentTree(
	        *AiScene, *AiScene->mRootNode, MaterialInstances, *Owner,
	        ShouldReplicate, ShouldRegisterComponentToOwner);

	// if ShouldRegisterComponentToOwner is ON
	if (ShouldRegisterComponentToOwner) {
		// register root to owning actor (Owner) to reflect in the unreal's scene
		ProceduralMeshComponentTreeRoot->RegisterComponent();
	}

	// return root ProceduralMeshComponent of ProceduralMeshComponentTree
	return ProceduralMeshComponentTreeRoot;
}

UStaticMeshComponent*
    UAssetConstructor::ConstructStaticMeshComponentFromAssetFile(
        const FString& FilePath, UMaterialInterface* ParentMaterialInterface,
        AActor* Owner,
        EConstructStaticMeshComponentFromAssetFileResult&
             ConstructStaticMeshComponentFromAssetFileResult,
        bool ShouldReplicate, bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// constrcut ProceduralMeshComponentTree
	EConstructProceduralMeshComponentFromAssetFileResult
	            ConstructProceduralMeshComponentFromAssetFileResult;
	const auto& ProceduralMeshComponentTreeRoot =
	    ConstructProceduralMeshComponentFromAssetFile(
	        FilePath, ParentMaterialInterface, Owner,
	        /*out */ ConstructProceduralMeshComponentFromAssetFileResult, false,
	        false);

	// if failed to load asset
	if (EConstructProceduralMeshComponentFromAssetFileResult::Failure ==
	    ConstructProceduralMeshComponentFromAssetFileResult) {
		// mark as failure
		ConstructStaticMeshComponentFromAssetFileResult =
		    EConstructStaticMeshComponentFromAssetFileResult::Failure;

		// return empty pointer
		return nullptr;
	}

	// construct Static Mesh Component Tree
	// from the root of the Procedural Mesh Component Tree
	const auto& StaticMeshComponentTreeRoot = ConstructStaticMeshComponentTree(
	    *ProceduralMeshComponentTreeRoot, *Owner, ShouldReplicate,
	    ShouldRegisterComponentToOwner);

	// if ShouldRegisterComponentToOwner is ON
	if (ShouldRegisterComponentToOwner) {
		// register root to owning actor (Owner) to reflect in the unreal's scene
		StaticMeshComponentTreeRoot->RegisterComponent();
	}

	// mark as success
	ConstructStaticMeshComponentFromAssetFileResult =
	    EConstructStaticMeshComponentFromAssetFileResult::Success;

	return StaticMeshComponentTreeRoot;
}

UDynamicMeshComponent*
    UAssetConstructor::ConstructDynamicMeshComponentFromAssetFile(
        const FString& FilePath, UMaterialInterface* ParentMaterialInterface,
        AActor* Owner,
        EConstructDynamicMeshComponentFromAssetFileResult&
             ConstructDynamicMeshComponentFromAssetFileResult,
        bool ShouldReplicate, bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// constrcut ProceduralMeshComponentTree
	EConstructProceduralMeshComponentFromAssetFileResult
	            ConstructProceduralMeshComponentFromAssetFileResult;
	const auto& ProceduralMeshComponentTreeRoot =
	    ConstructProceduralMeshComponentFromAssetFile(
	        FilePath, ParentMaterialInterface, Owner,
	        /*out */ ConstructProceduralMeshComponentFromAssetFileResult, false,
	        false);

	// if failed to load asset
	if (EConstructProceduralMeshComponentFromAssetFileResult::Failure ==
	    ConstructProceduralMeshComponentFromAssetFileResult) {
		// mark as failure
		ConstructDynamicMeshComponentFromAssetFileResult =
		    EConstructDynamicMeshComponentFromAssetFileResult::Failure;

		// return empty pointer
		return nullptr;
	}

	// construct Dynamic Mesh Component Tree
	// from the root of the Procedural Mesh Component Tree
	const auto& DynamicMeshComponentTreeRoot = ConstructDynamicMeshComponentTree(
	    *ProceduralMeshComponentTreeRoot, *Owner, ShouldReplicate,
	    ShouldRegisterComponentToOwner);

	// if ShouldRegisterComponentToOwner is ON
	if (ShouldRegisterComponentToOwner) {
		// register root to owning actor (Owner) to reflect in the unreal's scene
		DynamicMeshComponentTreeRoot->RegisterComponent();
	}

	// mark as success
	ConstructDynamicMeshComponentFromAssetFileResult =
	    EConstructDynamicMeshComponentFromAssetFileResult::Success;

	return DynamicMeshComponentTreeRoot;
}

#pragma region        definitions of static functions
static const aiScene* LoadAiScene(Assimp::Importer& AiImporter,
                                  const FString&    FilePath) {
	// import
	const auto& AiImportFlags =
	    aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
	    aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
	    aiProcess_OptimizeMeshes | aiProcess_RemoveRedundantMaterials |
	    aiProcess_ImproveCacheLocality | aiProcess_FindInvalidData |
	    aiProcess_EmbedTextures | aiProcess_GenUVCoords |
	    aiProcess_TransformUVCoords | aiProcess_MakeLeftHanded |
	    aiProcess_FlipUVs;

	return AiImporter.ReadFile(TCHAR_TO_UTF8(*FilePath), AiImportFlags);
}

static void TransformToUECoordinateSystem(const aiScene& AiScene) {
	// Generate a transformation matrix to transform from
	// the Ai(Assimp) coordinate system to the UE coordinate system.
	const auto& Ai_UE_XformMatrix = GenerateAi_UE_XformMatrix(AiScene);

	// get assimp root node
	auto& AiRootNode = AiScene.mRootNode;

	// get root node's transformation ref
	auto& AiRootNodeXForm = AiRootNode->mTransformation;

	// override assimp root node's transform
	AiRootNodeXForm = Ai_UE_XformMatrix * AiRootNodeXForm;
}

static float GetAiUnitScaleFactor(const aiScene& AiScene) {
	// get meta data of AiScene
	const auto& AiMetaData = AiScene.mMetaData;

	// if scene doesn't have meta data
	if (nullptr == AiMetaData) {
		return 1.0f;
	}

	// if scene has meta data, try to get meta data "UnitScaleFactor"
	float MetaDataUnitScaleFactor;
	bool  HasUnitScaleFactor =
	    AiMetaData->Get("UnitScaleFactor", /* out */ MetaDataUnitScaleFactor);

	// if there is not meta data "UnitScaleFactor"
	if (!HasUnitScaleFactor) {
		return 1.0f;
	}

	// if there is meta data "UnitScaleFactor", return it.
	return MetaDataUnitScaleFactor;
}

static aiMatrix4x4t<float> GenerateAi_UE_XformMatrix(const aiScene& AiScene) {
	// get AiUnitScaleFactor
	const float& AiUnitScaleFactor = GetAiUnitScaleFactor(AiScene);

	// Generate scaling matrix to convert from Assimp units to UE units
	aiMatrix4x4t<float> Scale_Ai_UE;
	aiMatrix4x4t<float>::Scaling(aiVector3t<float>(AiUnitScaleFactor),
	                             Scale_Ai_UE);

	// Generate a rotation matrix to convert from YUp in Assimp to ZUp in UE
	aiMatrix4x4t<float> Rot_AiYUp_UEZUp;
	aiMatrix4x4t<float>::RotationX(PI / 2.0f, Rot_AiYUp_UEZUp);

	return Scale_Ai_UE * Rot_AiYUp_UEZUp;
}

static TArray<UMaterialInstanceDynamic*>
    GenerateMaterialInstances(AActor& Owner, const aiScene& AiScene,
                              UMaterialInterface& ParentMaterialInterface) {
	TArray<UMaterialInstanceDynamic*> MaterialInstances;
	const auto&                       NumMaterials = AiScene.mNumMaterials;
	MaterialInstances.AddUninitialized(NumMaterials);

	if (0 == NumMaterials) {
		UE_LOG(LogAssetConstructor, Warning, TEXT("There is no Materials."));
	}
	for (auto i = decltype(NumMaterials){0}; i < NumMaterials; ++i) {
		// create material
		UMaterialInstanceDynamic* MaterialInstance =
		    UMaterialInstanceDynamic::Create(&ParentMaterialInterface, &Owner);

		const auto& AiMaterial = AiScene.mMaterials[i];
		const auto& NumTexture = AiMaterial->GetTextureCount(aiTextureType_DIFFUSE);

		// maybe, in case Vector4D Color is set
		if (0 == NumTexture) {
			// log that no texture is found
			UE_LOG(LogAssetConstructor, Log,
			       TEXT("No texture is found for material in index %d"), i);

			aiColor4D   AiDiffuse;
			const auto& GetDiffuseResult =
			    AiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, AiDiffuse);
			switch (GetDiffuseResult) {
			case aiReturn_FAILURE:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("No color is set for material in index %d"), i);
				break;
			case aiReturn_OUTOFMEMORY:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("Color couldn't get due to out of memory"));
				break;
			default:
				verifyf(aiReturn_SUCCESS == GetDiffuseResult,
				        TEXT("Bug. GetDiffuseResult should be aiReturn_SUCCESS."));

				VerifyMaterialParameter(ParentMaterialInterface,
				                        EMaterialParameterType::Vector, "BaseColor4");

				MaterialInstance->SetVectorParameterValue(
				    "BaseColor4",
				    FLinearColor{AiDiffuse.r, AiDiffuse.g, AiDiffuse.b, AiDiffuse.a});
				break;
			}
		}
		// if texture is set
		else {
			verifyf(NumTexture == 1,
			        TEXT("Currently, only one texture is supported "
			             "for diffuse (%d textures are found for material in "
			             "index %d)"),
			        NumTexture, i);

			aiString    AiTexture0Path;
			const auto& AiGetTextureResult = AiMaterial->Get(
			    AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), AiTexture0Path);
			switch (AiGetTextureResult) {
			case aiReturn_FAILURE:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("Failed to get texture for material in index %d"), i);
				break;
			case aiReturn_OUTOFMEMORY:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("Failed to get texture due to out of memory"));
				break;
			default:
				verifyf(aiReturn_SUCCESS == AiGetTextureResult,
				        TEXT("Bug. AiGetTextureResult should be aiReturn_SUCCESS."));
				const auto& AiTexture0 =
				    AiScene.GetEmbeddedTexture(AiTexture0Path.C_Str());

				if (nullptr == AiTexture0) {
					// TODO: load from file
					UE_LOG(LogAssetConstructor, Error,
					       TEXT("Texture %hs is not embedded in the file and "
					            "cannot be read."),
					       AiTexture0Path.C_Str());
				} else {
					const auto& Texture0 = [&AiTexture0]() {
						UTexture2D* Texture;

						const auto& Width  = AiTexture0->mWidth;
						const auto& Height = AiTexture0->mHeight;

						// if NOT compressed data
						if (Height != 0) {
							Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);

							if (Texture) {
								Texture->bNotOfflineProcessed = true;

								FTexturePlatformData* PlatformData =
#if ENGINE_MAJOR_VERSION > 4
								    Texture->GetPlatformData()
#else
								    Texture->PlatformData;
#endif
								    ;

								auto& TextureMip = PlatformData->Mips[0];
								auto& BulkData   = TextureMip.BulkData;
								auto* MipData    = BulkData.Lock(LOCK_READ_WRITE);
								FMemory::Memcpy(MipData, AiTexture0->pcData,
								                BulkData.GetBulkDataSize());

								BulkData.Unlock();

								Texture->UpdateResource();
							}
						} else {
							// when AiTexture0 is compressed
							const auto& Size = AiTexture0->mWidth;
							const auto& SeqData =
							    reinterpret_cast<const uint8*>(AiTexture0->pcData);
							const TArrayView64<const uint8> BufferView(SeqData, Size);

							Texture = FImageUtils::ImportBufferAsTexture2D(BufferView);
						}

						// if (Texture && bIsNormalMap) {
						//  Texture->CompressionSettings = TC_Normalmap;
						//  Texture->SRGB                = false;
						//  Texture->UpdateResource();
						//}

						return Texture;
					}();
					VerifyMaterialParameter(ParentMaterialInterface,
					                        EMaterialParameterType::Texture,
					                        "BaseColorTexture");

					MaterialInstance->SetTextureParameterValue("BaseColorTexture",
					                                           Texture0);
				}

				break;
			}
#if 0
				MaterialInstance->SetScalarParameterValue("Normal", AiNormalTexture);
#endif
		}

		MaterialInstances[i] = MaterialInstance;
	}

	return MaterialInstances;
}

static TArray<FLoadedMaterialData>
    GenerateMaterialDatas(const aiScene& AiScene) {
	TArray<FLoadedMaterialData> MaterialList;
	const auto&                 NumMaterials = AiScene.mNumMaterials;
	MaterialList.AddDefaulted(NumMaterials);

	if (0 == NumMaterials) {
		UE_LOG(LogAssetConstructor, Warning, TEXT("There is no Materials."));
	}
	for (auto i = decltype(NumMaterials){0}; i < NumMaterials; ++i) {
		// get reference of the material
		auto& MaterialData = MaterialList[i];

		// get ai(assimp) material
		const auto& AiMaterial = AiScene.mMaterials[i];

		// get number of textures
		const auto& NumTexture = AiMaterial->GetTextureCount(aiTextureType_DIFFUSE);

		// maybe, in case Vector4D Color is set
		if (0 == NumTexture) {
			// log that no texture is found
			UE_LOG(LogAssetConstructor, Log,
			       TEXT("No texture is found for material in index %d"), i);

			aiColor4D   AiDiffuse;
			const auto& GetDiffuseResult =
			    AiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, AiDiffuse);
			switch (GetDiffuseResult) {
			case aiReturn_FAILURE:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("No color is set for material in index %d"), i);
				break;
			case aiReturn_OUTOFMEMORY:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("Color couldn't get due to out of memory"));
				break;
			default:
				verifyf(aiReturn_SUCCESS == GetDiffuseResult,
				        TEXT("Bug. GetDiffuseResult should be aiReturn_SUCCESS."));

				MaterialData.Color =
				    FLinearColor{AiDiffuse.r, AiDiffuse.g, AiDiffuse.b, AiDiffuse.a};
				break;
			}
		}
		// if texture is set
		else {
			verifyf(NumTexture == 1,
			        TEXT("Currently, only one texture is supported "
			             "for diffuse (%d textures are found for material in "
			             "index %d)"),
			        NumTexture, i);

			aiString    AiTexture0Path;
			const auto& AiGetTextureResult = AiMaterial->Get(
			    AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), AiTexture0Path);
			switch (AiGetTextureResult) {
			case aiReturn_FAILURE:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("Failed to get texture for material in index %d"), i);
				break;
			case aiReturn_OUTOFMEMORY:
				UE_LOG(LogAssetConstructor, Warning,
				       TEXT("Failed to get texture due to out of memory"));
				break;
			default:
				verifyf(aiReturn_SUCCESS == AiGetTextureResult,
				        TEXT("Bug. AiGetTextureResult should be aiReturn_SUCCESS."));
				const auto& AiTexture0 =
				    AiScene.GetEmbeddedTexture(AiTexture0Path.C_Str());

				if (nullptr == AiTexture0) {
					// TODO: load from file
					UE_LOG(LogAssetConstructor, Error,
					       TEXT("Texture %hs is not embedded in the file and "
					            "cannot be read."),
					       AiTexture0Path.C_Str());
				} else {
					// get width and height
					const auto& Width  = AiTexture0->mWidth;
					const auto& Height = AiTexture0->mHeight;

					// if NOT compressed data
					if (Height != 0) {
						TArray64<uint8> CompressedTextureData;

						FImageView ImageView(AiTexture0->pcData, Width, Height,
						                     ERawImageFormat::BGRA8);
						FImageUtils::CompressImage(CompressedTextureData, TEXT("png"),
						                           ImageView);

						MaterialData.CompressedTextureData =
						    MoveTemp(CompressedTextureData);
					}
					// if compressed data
					else {
						// when AiTexture0 is compressed
						const auto& Size = AiTexture0->mWidth;
						const auto& SeqData =
						    reinterpret_cast<const uint8*>(AiTexture0->pcData);

						MaterialData.CompressedTextureData =
						    decltype(MaterialData.CompressedTextureData)(SeqData, Size);
					}
					MaterialData.HasTexture = true;
				}

				break;
			}
		}

		MaterialList[i] = MaterialData;
	}

	return MaterialList;
}

TArray<UMaterialInstanceDynamic*>
    GenerateMaterialInstances(AActor&                            Owner,
                              const TArray<FLoadedMaterialData>& MaterialDatas,
                              UMaterialInterface& ParentMaterialInterface) {
	TArray<UMaterialInstanceDynamic*> MaterialInstances;
	const auto&                       NumMaterials = MaterialDatas.Num();
	MaterialInstances.AddUninitialized(NumMaterials);

	if (0 == NumMaterials) {
		UE_LOG(LogAssetConstructor, Warning, TEXT("There is no Materials."));
	}
	for (auto i = decltype(NumMaterials){0}; i < NumMaterials; ++i) {
		// create material
		UMaterialInstanceDynamic* MaterialInstance =
		    UMaterialInstanceDynamic::Create(&ParentMaterialInterface, &Owner);

		const auto& MaterialData = MaterialDatas[i];

		// in case Color is set (no texture)
		if (!MaterialData.HasTexture) {
			// log that no texture is found
			UE_LOG(LogAssetConstructor, Log,
			       TEXT("No texture is found for material in index %d"), i);

			VerifyMaterialParameter(ParentMaterialInterface,
			                        EMaterialParameterType::Vector, "BaseColor4");

			// get color
			const auto& Color = MaterialData.Color;

			// set color
			MaterialInstance->SetVectorParameterValue("BaseColor4", Color);
		}
		// if texture is set
		else {
			// get compressed texture data
			const auto& CompressedTextureData = MaterialData.CompressedTextureData;

			// get texture
			UTexture2D* Texture0 =
			    FImageUtils::ImportBufferAsTexture2D(CompressedTextureData);

			VerifyMaterialParameter(ParentMaterialInterface,
			                        EMaterialParameterType::Texture,
			                        "BaseColorTexture");

			// set texture
			MaterialInstance->SetTextureParameterValue("BaseColorTexture", Texture0);
		}

		MaterialInstances[i] = MaterialInstance;
	}

	return MaterialInstances;
}

static void
    VerifyMaterialParameter(const UMaterialInterface&     MaterialInterface,
                            const EMaterialParameterType& MaterialParameterType,
                            const FName&                  ParameterName) {
	FMemoryImageMaterialParameterInfo MemoryImageMaterialParameterInfo(
	    ParameterName);
	FMaterialParameterMetadata MaterialParameterMetadata;

	// check if parameter exists
	const auto& ParameterExists = MaterialInterface.GetParameterDefaultValue(
	    MaterialParameterType, MemoryImageMaterialParameterInfo,
	    MaterialParameterMetadata);

	// verify
	verifyf(ParameterExists, TEXT("Material %s doesn't have %s parameter."),
	        *MaterialInterface.GetFName().ToString(), *ParameterName.ToString());
}

static UProceduralMeshComponent* ConstructProceduralMeshComponentTree(
    const aiScene& AiScene, const aiNode& AiNode,
    const TArray<UMaterialInstanceDynamic*>& MaterialInstances, AActor& Owner,
    const bool ShouldReplicate, const bool ShouldRegisterComponentToOwner) {
	//// get node name
	// const auto& AiNodeName = AiNode.mName;

	// new ProceduralMeshComponent
	const auto& ProcMeshComp = NewObject<UProceduralMeshComponent>(&Owner);

	// set RelativeTransform
	const auto& AiTransformMatrix = AiNode.mTransformation;
	ProcMeshComp->SetRelativeTransform(
	    static_cast<FTransform>(AiMatrixToUEMatrix(AiTransformMatrix)));

	// make MeshComponent network addressable
	ProcMeshComp->SetNetAddressable();

	// set whether MeshComponent should be replicated
	ProcMeshComp->SetIsReplicated(ShouldReplicate);

	// create mesh sections
	const auto& NumMeshes = AiNode.mNumMeshes;
	for (auto i = decltype(NumMeshes){0}; i < NumMeshes; ++i) {
		// get assimp mesh
		const auto& AiMeshIndex = AiNode.mMeshes[i];
		const auto& AiMesh      = AiScene.mMeshes[AiMeshIndex];

		// convert to unreal Vertex format
		TArray<FVector> Vertices = [&AiMesh]() {
			TArray<FVector> Vertices;
			const auto&     NumVertices = AiMesh->mNumVertices;
			Vertices.AddUninitialized(NumVertices);
			const auto& AiVertices = AiMesh->mVertices;

			if (!AiMesh->HasPositions()) {
				UE_LOG(LogAssetConstructor, Warning, TEXT("There is no Vertices."));
			} else {
				check(NumVertices > 0 && AiVertices != nullptr);
				for (auto i = decltype(NumVertices){0}; i < NumVertices; ++i) {
					const auto& AiVertex = AiVertices[i];
					Vertices[i]          = {AiVertex.x, AiVertex.y, AiVertex.z};
				}
			}

			return Vertices;
		}();

		// convert to unreal Triangle format
		TArray<int32> Triangles = [&AiMesh]() {
			TArray<int32> Triangles;
			const auto&   NumFaces = AiMesh->mNumFaces;
			Triangles.AddUninitialized(NumFaces * 3);
			const auto& AiFaces = AiMesh->mFaces;

			if (!AiMesh->HasFaces()) {
				UE_LOG(LogAssetConstructor, Warning, TEXT("There is no Faces."));
			} else {
				check(NumFaces > 0 && AiFaces != nullptr);
				for (auto i = decltype(NumFaces){0}; i < NumFaces; ++i) {
					const auto& AiFace = AiFaces[i];
					checkf(AiFace.mNumIndices == 3,
					       TEXT("Each face must be triangular."));

					for (int_fast8_t triangle_i = 0; triangle_i < 3; ++triangle_i) {
						Triangles[3 * i + triangle_i] = AiFace.mIndices[triangle_i];
					}
				}
			}

			return Triangles;
		}();

		// convert to unreal Normal format
		TArray<FVector> Normals = [&AiMesh]() {
			TArray<FVector> Normals;
			const auto&     NumNormals =
			    AiMesh->mNumVertices; // num of Normals == num of Vertices
			Normals.AddUninitialized(NumNormals);
			const auto& AiNormals = AiMesh->mNormals;

			if (!AiMesh->HasNormals()) {
				UE_LOG(LogAssetConstructor, Log, TEXT("There is no Normal datas."));
			} else {
				check(NumNormals > 0 && AiNormals != nullptr);
				for (auto i = decltype(NumNormals){0}; i < NumNormals; ++i) {
					const auto& AiNormal = AiNormals[i];
					Normals[i]           = {AiNormal.x, AiNormal.y, AiNormal.z};
				}
			}

			return Normals;
		}();

		// convert to unreal UV0 format
		TArray<FVector2D> UV0Channel = [&AiMesh]() {
			TArray<FVector2D> UV0Channel;
			const auto&       NumVertices = AiMesh->mNumVertices;
			UV0Channel.AddUninitialized(NumVertices);
			const auto& AiUVChannels = AiMesh->mTextureCoords;

			const auto& NumUVChannels = AiMesh->GetNumUVChannels();

			// if there is no UV Channels
			if (!AiMesh->HasTextureCoords(0)) {
				// log
				UE_LOG(LogAssetConstructor, Log, TEXT("There is no UV channels."));
			} else {
				check(NumUVChannels > 0 && AiUVChannels != nullptr);
				ensureMsgf(1 == NumUVChannels,
				           TEXT("Currently only 1 UV channel is supported."));

				const auto& AiUV0Channel = AiUVChannels[0];
				if (0 == NumVertices || nullptr == AiUV0Channel) {
					check(0 == NumVertices && nullptr == AiUV0Channel);
					// log
					UE_LOG(LogAssetConstructor, Log,
					       TEXT("The first UV channel exists but there is no vertex or "
					            "channel "
					            "data."));
				} else {
					for (auto i = decltype(NumVertices){0}; i < NumVertices; ++i) {
						const auto& AiUV0 = AiUV0Channel[i];
						UV0Channel[i]     = {AiUV0.x, AiUV0.y};
					}
				}
			}

			return UV0Channel;
		}();

		// convert to unreal Vertex Color format
		TArray<FLinearColor> VertexColors0 = [&AiMesh]() {
			TArray<FLinearColor> VertexColors0;
			const auto&          NumVertices = AiMesh->mNumVertices;
			VertexColors0.AddUninitialized(NumVertices);
			const auto& AiVertexColors = AiMesh->mColors;

			const auto& NumVertexColorChannels = AiMesh->GetNumColorChannels();

			// if there is no Vertex Color Channels
			if (!AiMesh->HasVertexColors(0)) {
				// log
				UE_LOG(LogAssetConstructor, Log,
				       TEXT("There is no Vertex Color channels."));
			} else {
				check(NumVertexColorChannels > 0 && AiVertexColors != nullptr);
				ensureMsgf(1 == NumVertexColorChannels,
				           TEXT("Currently only 1 Vertex Color channel is supported."));

				const auto& AiVertexColors0 = AiVertexColors[0];
				if (0 == NumVertices || nullptr == AiVertexColors0) {
					check(0 == NumVertices && nullptr == AiVertexColors0);
					// log
					UE_LOG(LogAssetConstructor, Log,
					       TEXT("The first Vertex Color channel exists but there is no "
					            "vertex or "
					            "channel data."));
				} else {
					for (auto i = decltype(NumVertices){0}; i < NumVertices; ++i) {
						const auto& AiVertexColor = AiVertexColors0[i];
						VertexColors0[i]          = {AiVertexColor.r, AiVertexColor.g,
						                             AiVertexColor.b, AiVertexColor.a};
					}
				}
			}

			return VertexColors0;
		}();

		// convert to unreal Tangent format
		TArray<FProcMeshTangent> Tangents = [&AiMesh]() {
			TArray<FProcMeshTangent> Tangents;
			const auto&              NumTangents =
			    AiMesh->mNumVertices; // num of Tangents == num of Vertices
			Tangents.AddUninitialized(NumTangents);
			const auto& AiTangents = AiMesh->mTangents;

			if (!AiMesh->HasTangentsAndBitangents()) {
				UE_LOG(LogAssetConstructor, Log, TEXT("There is no Tangent datas."));
			} else {
				check(NumTangents > 0 && AiTangents != nullptr);
				for (auto i = decltype(NumTangents){0}; i < NumTangents; ++i) {
					const auto& AiTangent = AiTangents[i];
					Tangents[i]           = {AiTangent.x, AiTangent.y, AiTangent.z};
				}
			}

			return Tangents;
		}();

		// CreateCollision parameter
		constexpr auto CreateCollision = true;

		// bSRGBCConversion parameter
		constexpr auto bSRGBCConversion = false;

		// create mesh section
		ProcMeshComp->CreateMeshSection_LinearColor(
		    i, Vertices, Triangles, Normals, UV0Channel, VertexColors0, Tangents,
		    CreateCollision, bSRGBCConversion);

		// set Material
		const auto& MaterialIndex    = AiMesh->mMaterialIndex;
		const auto& MaterialInstance = MaterialInstances[MaterialIndex];
		ProcMeshComp->SetMaterial(i, MaterialInstance);
	}

	// Recursively construct children's ProceduralMeshComponentTree
	const auto& NumChildren = AiNode.mNumChildren;
	for (auto i = decltype(NumChildren){0}; i < NumChildren; ++i) {
		// get assimp child Node
		const auto& AiChildNode = *AiNode.mChildren[i];

		// construct ChildProcMeshComponent
		const auto& ChildProcMeshComponent = ConstructProceduralMeshComponentTree(
		    AiScene, AiChildNode, MaterialInstances, Owner, ShouldReplicate,
		    ShouldRegisterComponentToOwner);

		// if ShouldRegisterComponentToOwner is ON
		if (ShouldRegisterComponentToOwner) {
			// Setup of attachment of child component to ProcMeshComp
			ChildProcMeshComponent->SetupAttachment(ProcMeshComp);

			// register component to owning actor (Owner) to reflect in the unreal's
			// scene
			ChildProcMeshComponent->RegisterComponent();
		}
		// if ShouldRegisterComponentToOwner is OFF
		else {
			// attach child component to ProcMeshComp
			ChildProcMeshComponent->AttachToComponent(
			    ProcMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	return ProcMeshComp;
}

static FMatrix AiMatrixToUEMatrix(const aiMatrix4x4& AiMatrix4x4) {
	// give a short name
	const auto& M = AiMatrix4x4;

	// return transpose of assimp matrix
	return {{M.a1, M.b1, M.c1, M.d1},
	        {M.a2, M.b2, M.c2, M.d2},
	        {M.a3, M.b3, M.c3, M.d3},
	        {M.a4, M.b4, M.c4, M.d4}};
}

static UStaticMeshComponent* ConstructStaticMeshComponentTree(
    UProceduralMeshComponent& ProceduralMeshComponentNode, AActor& Owner,
    const bool ShouldReplicate, const bool ShouldRegisterComponentToOwner) {
	// new StaticMeshComponent
	const auto& StaticMeshComponent = NewObject<UStaticMeshComponent>(&Owner);

	// copy RelativeTransform
	StaticMeshComponent->SetRelativeTransform(
	    ProceduralMeshComponentNode.GetRelativeTransform());

	// make MeshComponent network addressable
	StaticMeshComponent->SetNetAddressable();

	// set whether MeshComponent should be replicated
	StaticMeshComponent->SetIsReplicated(ShouldReplicate);

	// if there are mesh sections
	if (const auto& NumMeshSections =
	        ProceduralMeshComponentNode.GetNumSections();
	    NumMeshSections > 0) {
		// new StaticMesh
		const auto& StaticMesh      = NewObject<UStaticMesh>(&Owner);
		StaticMesh->bAllowCPUAccess = true;
		StaticMesh->NeverStream     = true;
		StaticMesh->InitResources();
		StaticMesh->SetLightingGuid();

		// copy meshes
		{
			// get description of ProceduralMesh
			auto ProceduralMeshDescription =
			    BuildMeshDescription(&ProceduralMeshComponentNode);

			// create parameters of BuildFromMeshDescriptions function
			UStaticMesh::FBuildMeshDescriptionsParams BuildMeshDescriptionsParams;
#if !WITH_EDITOR
			BuildMeshDescriptionsParams.bFastBuild =
			    true; // set fast build (mandatory in non-editor builds)
			BuildMeshDescriptionsParams.bAllowCpuAccess = true;
#endif

			// copy meshes from ProceduralMeshComponent
			StaticMesh->BuildFromMeshDescriptions({&ProceduralMeshDescription},
			                                      BuildMeshDescriptionsParams);
		}

		// copy collisions
		StaticMesh->CalculateExtendedBounds();
		StaticMesh->SetBodySetup(ProceduralMeshComponentNode.ProcMeshBodySetup);

		// copy materials
		for (const auto& MaterialInterface :
		     ProceduralMeshComponentNode.GetMaterials()) {
			StaticMesh->AddMaterial(MaterialInterface);
		}

#if WITH_EDITOR
		StaticMesh->PostEditChange();
#endif

		StaticMesh->MarkPackageDirty();

		// set static mesh
		StaticMeshComponent->SetStaticMesh(StaticMesh);
	}

	// Recursively construct children's StaticMeshComponentTree
	const auto& Children    = ProceduralMeshComponentNode.GetAttachChildren();
	const auto& NumChildren = Children.Num();
	for (auto i = decltype(NumChildren){0}; i < NumChildren; ++i) {
		// get child component
		auto& ChildProcMeshCompNode =
		    dynamic_cast<UProceduralMeshComponent&>(*Children[i]);

		// construct ChildStaticMeshComponent
		const auto& ChildStaticMeshComponent = ConstructStaticMeshComponentTree(
		    ChildProcMeshCompNode, Owner, ShouldReplicate,
		    ShouldRegisterComponentToOwner);

		// if ShouldRegisterComponentToOwner is ON
		if (ShouldRegisterComponentToOwner) {
			// Setup of attachment of child component to ProcMeshComp
			ChildStaticMeshComponent->SetupAttachment(StaticMeshComponent);

			// register component to owning actor (Owner) to reflect in the unreal's
			// scene
			ChildStaticMeshComponent->RegisterComponent();
		}
		// if ShouldRegisterComponentToOwner is OFF
		else {
			// attach child component to ProcMeshComp
			ChildStaticMeshComponent->AttachToComponent(
			    StaticMeshComponent,
			    FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	return StaticMeshComponent;
}

static UDynamicMeshComponent* ConstructDynamicMeshComponentTree(
    UProceduralMeshComponent& ProceduralMeshComponentNode, AActor& Owner,
    const bool ShouldReplicate, const bool ShouldRegisterComponentToOwner) {
	// new DynamicMeshComponent
	const auto& DynamicMeshComponent = NewObject<UDynamicMeshComponent>(&Owner);

	// copy RelativeTransform
	DynamicMeshComponent->SetRelativeTransform(
	    ProceduralMeshComponentNode.GetRelativeTransform());

	// set materials
	DynamicMeshComponent->ConfigureMaterialSet(
	    ProceduralMeshComponentNode.GetMaterials());

	// make MeshComponent network addressable
	DynamicMeshComponent->SetNetAddressable();

	// set whether MeshComponent should be replicated
	DynamicMeshComponent->SetIsReplicated(ShouldReplicate);

	// if there are mesh sections
	if (const auto& NumMeshSections =
	        ProceduralMeshComponentNode.GetNumSections();
	    NumMeshSections > 0) {
		// copy meshes from Procedural Mesh
		auto ProceduralMeshDescription =
		    BuildMeshDescription(&ProceduralMeshComponentNode);
		UE::Geometry::FDynamicMesh3   DynamicMesh3;
		FMeshDescriptionToDynamicMesh MeshDescriptionToDynamicMesh;
		MeshDescriptionToDynamicMesh.Convert(&ProceduralMeshDescription,
		                                     DynamicMesh3, true);

		// enable collisions
		DynamicMeshComponent->EnableComplexAsSimpleCollision();
		DynamicMeshComponent->SetCollisionProfileName(
		    UCollisionProfile::BlockAllDynamic_ProfileName);

		// set
		DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh3));
	}

	// Recursively construct children's DynamicMeshComponentTree
	const auto& Children    = ProceduralMeshComponentNode.GetAttachChildren();
	const auto& NumChildren = Children.Num();
	for (auto i = decltype(NumChildren){0}; i < NumChildren; ++i) {
		// get child component
		auto& ChildProcMeshCompNode =
		    dynamic_cast<UProceduralMeshComponent&>(*Children[i]);

		// construct ChildDynamicMeshComponent
		const auto& ChildDynamicMeshComponent = ConstructDynamicMeshComponentTree(
		    ChildProcMeshCompNode, Owner, ShouldReplicate,
		    ShouldRegisterComponentToOwner);

		// if ShouldRegisterComponentToOwner is ON
		if (ShouldRegisterComponentToOwner) {
			// Setup of attachment of child component to DynamicMeshComponent
			ChildDynamicMeshComponent->SetupAttachment(DynamicMeshComponent);

			// register component to owning actor (Owner) to reflect in the unreal's
			// scene
			ChildDynamicMeshComponent->RegisterComponent();
		}
		// if ShouldRegisterComponentToOwner is OFF
		else {
			// attach child component to DynamicMeshComponent
			ChildDynamicMeshComponent->AttachToComponent(
			    DynamicMeshComponent,
			    FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	return DynamicMeshComponent;
}
#pragma endregion
