// Fill out your copyright notice in the Description page of Project Settings.

#include "AssetConstructor.h"

#include "AssetLoader.h"
#include "ImageUtils.h"
#include "LogAssetConstructor.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "ProceduralMeshConversion.h"

#include <type_traits>

#pragma region forward declarations of static functions
/**
 * Generate material instances from array of material data.
 * @param Owner Owner of the material instances
 * @param MaterialDataList array of material data
 * @param ParentMaterialInterface Parent MaterialInterface from which
 *                                the material instance was created
 * @return array of the material instances
 */
static TArray<UMaterialInstanceDynamic*>
    GenerateMaterialInstances(UObject&                           Owner,
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
 * template function to construct specified mesh component from mesh data.
 * @tparam  MeshComponentT UProceduralMesh/UStaticMesh/UDynamicMesh
 * @param   MeshData                    loaded mesh data
 * @param   ParentMaterialInterface     The base material interface used to
 *                                      create materials for the constructed
 *                                      meshes.
 * @param   Owner                       Owner of the returned dynamic mesh
 *                                      component, its descendants and its
 *                                      material instances.
 * @param   ShouldRegisterComponentToOwner    Whether to register components
 *                                            to Owner. Must be turned ON to
 *                                            be reflected in the scene.
 */
template <typename MeshComponentT>
static MeshComponentT* ConstructMeshComponentFromMeshData(
    const FLoadedMeshData&    MeshData,
    UMaterialInterface* const ParentMaterialInterface, AActor* const Owner,
    const bool ShouldRegisterComponentToOwner) {
	// check that the NodeList in MeshData has at least one node (because there
	// must be a root node)
	check(!MeshData.NodeList.IsEmpty());

	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// get node list
	const auto& NodeList = MeshData.NodeList;

	// number of the NodeList
	const auto& NumNodeList = NodeList.Num();

	// list of mesh components to be made
	TArray<MeshComponentT*> MeshComponentList;
	MeshComponentList.AddUninitialized(NumNodeList);

	// get material list
	const auto& MaterialList = MeshData.MaterialList;

	// generate material instances
	const auto& MaterialInstances =
	    GenerateMaterialInstances(*Owner, MaterialList, *ParentMaterialInterface);

	// construct Mesh Component Tree
	for (auto Node_i = decltype(NumNodeList){0}; Node_i < NumNodeList; ++Node_i) {
		// get reference of the node
		const auto& Node = NodeList[Node_i];

		// new MeshComponent
		const auto& MeshComponent = NewObject<MeshComponentT>(Owner);

		// set RelativeTransform
		MeshComponent->SetRelativeTransform(Node.RelativeTransform);

		// make MeshComponent network addressable
		MeshComponent->SetNetAddressable();

		// get reference of the sections
		const auto& Sections = Node.Sections;

		// get number of sections
		const auto& NumSections = Sections.Num();

		// create mesh sections
		if constexpr (TypeTests::TAreTypesEqual_V<UProceduralMeshComponent,
		                                          MeshComponentT>) {
			for (auto Section_i = decltype(NumSections){0}; Section_i < NumSections;
			     ++Section_i) {
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
			}
		} else {
			// create transient Procedural Mesh Component
			const auto& SrcProcMeshComp = NewObject<UProceduralMeshComponent>(Owner);

			// set RelativeTransform
			SrcProcMeshComp->SetRelativeTransform(Node.RelativeTransform);

			// create meshes of Procedural Mesh Component
			for (auto Section_i = decltype(NumSections){0}; Section_i < NumSections;
			     ++Section_i) {
				// get reference of the section
				const auto& Section = Sections[Section_i];

				// CreateCollision parameter
				constexpr auto CreateCollision = true;

				// bSRGBCConversion parameter
				constexpr auto bSRGBCConversion = false;

				// create mesh section
				SrcProcMeshComp->CreateMeshSection_LinearColor(
				    Section_i, Section.Vertices, Section.Triangles, Section.Normals,
				    Section.UV0Channel, Section.VertexColors0, Section.Tangents,
				    CreateCollision, bSRGBCConversion);

				// set Material
				const auto& MaterialIndex    = Section.MaterialIndex;
				const auto& MaterialInstance = MaterialInstances[MaterialIndex];
				SrcProcMeshComp->SetMaterial(Section_i, MaterialInstance);
			}

			// get description of ProceduralMesh
			auto ProceduralMeshDescription = BuildMeshDescription(SrcProcMeshComp);

			if constexpr (TypeTests::TAreTypesEqual_V<UStaticMeshComponent,
			                                          MeshComponentT>) {
				// new StaticMesh
				const auto& StaticMesh      = NewObject<UStaticMesh>(Owner);
				StaticMesh->bAllowCPUAccess = true;
				StaticMesh->NeverStream     = true;
				StaticMesh->InitResources();
				StaticMesh->SetLightingGuid();

				// copy meshes
				// create parameters of BuildFromMeshDescriptions function
				{
					UStaticMesh::FBuildMeshDescriptionsParams BuildMeshDescriptionsParams;
#if !WITH_EDITOR
					BuildMeshDescriptionsParams.bFastBuild =
					    true; // set fast build (mandatory in non-editor builds)
					BuildMeshDescriptionsParams.bAllowCpuAccess = true;
#endif

					// copy meshes from SrcProcMeshComp
					StaticMesh->BuildFromMeshDescriptions({&ProceduralMeshDescription},
					                                      BuildMeshDescriptionsParams);
				}

				// copy collisions
				StaticMesh->CalculateExtendedBounds();
				StaticMesh->SetBodySetup(SrcProcMeshComp->ProcMeshBodySetup);

				// copy materials
				for (const auto& MaterialInterface : SrcProcMeshComp->GetMaterials()) {
					StaticMesh->AddMaterial(MaterialInterface);
				}

#if WITH_EDITOR
				StaticMesh->PostEditChange();
#endif

				StaticMesh->MarkPackageDirty();

				// set static mesh
				MeshComponent->SetStaticMesh(StaticMesh);
			} else if constexpr (TypeTests::TAreTypesEqual_V<UDynamicMeshComponent,
			                                                 MeshComponentT>) {
				UE::Geometry::FDynamicMesh3   DynamicMesh3;
				FMeshDescriptionToDynamicMesh MeshDescriptionToDynamicMesh;
				MeshDescriptionToDynamicMesh.Convert(&ProceduralMeshDescription,
				                                     DynamicMesh3, true);

				// enable collisions
				MeshComponent->EnableComplexAsSimpleCollision();
				MeshComponent->SetCollisionProfileName(
				    UCollisionProfile::BlockAllDynamic_ProfileName);

				// set materials
				MeshComponent->ConfigureMaterialSet(SrcProcMeshComp->GetMaterials());

				// set
				MeshComponent->SetMesh(MoveTemp(DynamicMesh3));
			} else {
				// type check error
				static_assert(
				    []() {
					    return false;
				    }(),
				    "Only UProceduralMeshComponent, UStaticMeshComponent or "
				    "UDynamicMeshComponent is "
				    "supported for MeshComponentT.");
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
#pragma endregion

void UAssetConstructor::CreateMeshFromMeshDataOnProceduralMeshComponent(
    const FLoadedMeshData&    MeshData,
    UMaterialInterface*       ParentMaterialInterface,
    UProceduralMeshComponent* TargetProceduralMeshComponent) {
	// check that the NodeList in MeshData has at least one node (because there
	// must be a root node)
	check(!MeshData.NodeList.IsEmpty());

	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to TargetProceduralMeshComponent is properly set
	check(TargetProceduralMeshComponent != nullptr);

	// get node list
	const auto& NodeList = MeshData.NodeList;

	// number of the NodeList
	const auto& NumNodeList = NodeList.Num();

	// List of transforms relative to the origin of TargetProceduralMeshComponent
	TArray<FTransform> TransformListToTarget;
	TransformListToTarget.AddUninitialized(NumNodeList);

	// get material list
	const auto& MaterialList = MeshData.MaterialList;

	// generate material instances
	const auto& MaterialInstances = GenerateMaterialInstances(
	    *TargetProceduralMeshComponent, MaterialList, *ParentMaterialInterface);

	// index of a mesh section in TargetProceduralMeshComponent
	int32 MeshSectionIndex = 0;

	// construct Mesh Component Tree
	for (auto Node_i = decltype(NumNodeList){0}; Node_i < NumNodeList; ++Node_i) {
		// get reference of the node
		const auto& Node = NodeList[Node_i];

		// get RelativeTransform
		const auto& RelativeTransform = Node.RelativeTransform;

		// get parent node index
		const auto& ParentNodeIndex = Node.ParentNodeIndex;

		// get parent's transform relative to target
		const auto& ParentTransformToTarget = [&Node_i, &TransformListToTarget,
		                                       &ParentNodeIndex]() {
			// if this node is a root node
			if (0 == Node_i) {
				// return identity transform matrix
				return FTransform::Identity;
			}

			// if this node is a non-root node, return parent's transform relative to
			// target.
			return TransformListToTarget[ParentNodeIndex];
		}();

		// calculate transform relative to target
		// To convert local coordinates to world coordinates:
		//   Assume the following parent-child relationship
		//     Child1 (parent) - Child2 (child) - Child3 (grandchild)
		//
		//   Let
		//    - DVn (Translation) be the relative coordinate vector of
		//      Child n to its parent
		//    - Rn (Rotation) be the relative rotation vector of ...
		//    - Sn (Scale) be the relative scale vector of ...
		//   and represent them in a single matrix:
		//    - "Transform(n)" be the transformation matrix
		//      (If n is just a number, I omit the parentheses)
		//
		//   then, let the absolute coordinate vector Vn be,
		//   V3 = S1*R1*(S2*R2*DV3 + DV2) + DV1
		//      = S1*R1*(Transform2*DV3) + DV1
		//      = Transform1*(Transform2*DV3)
		//      = (Transform1 * Transform2) * DV3
		//
		// So the transformation matrix that transforms relative coordinate of Child
		// n to absolute coordinate can be calculated by
		//    Transform1 * Transform2 * ... * Transform(n-1)              ...... <1>
		//
		// However, in the Unreal Engine, the transformation matrix is held in the
		// form of a transposed matrix. For any matrix A, let t(A) be its transpose
		// matrix, then the formula <1> equals
		//    t(t(Transform(n-1)) * ... * t(Transform2) * t(Transform1))
		//
		// Therefore, I calculate as follows:
		const auto& TransformToTarget =
		    FTransform(RelativeTransform.ToMatrixWithScale() *
		               ParentTransformToTarget.ToMatrixWithScale());

		// set to transform list
		TransformListToTarget[Node_i] = TransformToTarget;

		// rotator of TransformToTarget
		const auto& TransformToTargetRotator = TransformToTarget.Rotator();

		// get reference of the sections
		const auto& Sections = Node.Sections;

		// get number of sections
		const auto& NumSections = Sections.Num();

		// create mesh sections
		for (auto Section_i = decltype(NumSections){0}; Section_i < NumSections;
		     ++Section_i) {
			// get reference of the section
			const auto& Section = Sections[Section_i];

			// CreateCollision parameter
			constexpr auto CreateCollision = true;

			// bSRGBCConversion parameter
			constexpr auto bSRGBConversion = false;

			// get Vertices relative to my parent node
			const auto& Vertices = Section.Vertices;

			// generate Vertices relative to TargetProceduralMeshComponent
			auto VerticesRelativeToTarget = decltype(Vertices){};
			Algo::Transform(Vertices, VerticesRelativeToTarget,
			                [&TransformToTarget](const FVector& vector) {
				                return TransformToTarget.TransformFVector4(vector);
			                });

			// get Normals relative to my parent node
			const auto& Normals = Section.Normals;

			// generate Normals relative to TargetProceduralMeshComponent
			auto NormalsRelativeToTarget = decltype(Normals){};
			Algo::Transform(Normals, NormalsRelativeToTarget,
			                [&TransformToTargetRotator](const FVector& vector) {
				                return TransformToTargetRotator.RotateVector(vector);
			                });

			// get Tangents relative to my parent node
			const auto& Tangents = Section.Tangents;

			// generate Tangents relative to TargetProceduralMeshComponent
			auto TangentsRelativeToTarget = decltype(Tangents){};
			Algo::Transform(
			    Tangents, TangentsRelativeToTarget,
			    [&TransformToTargetRotator](const FProcMeshTangent& ProcMeshTangent) {
				    return FProcMeshTangent(
				        TransformToTargetRotator.RotateVector(ProcMeshTangent.TangentX),
				        ProcMeshTangent.bFlipTangentY);
			    });

			// create mesh section
			TargetProceduralMeshComponent->CreateMeshSection_LinearColor(
			    MeshSectionIndex, VerticesRelativeToTarget, Section.Triangles,
			    NormalsRelativeToTarget, Section.UV0Channel, Section.VertexColors0,
			    TangentsRelativeToTarget, CreateCollision, bSRGBConversion);

			// set Material
			const auto& MaterialIndex    = Section.MaterialIndex;
			const auto& MaterialInstance = MaterialInstances[MaterialIndex];
			TargetProceduralMeshComponent->SetMaterial(MeshSectionIndex,
			                                           MaterialInstance);

			// increment index of a mesh section in TargetProceduralMeshComponent
			MeshSectionIndex++;
		}
	}
}

UProceduralMeshComponent*
    UAssetConstructor::ConstructProceduralMeshComponentFromMeshData(
        const FLoadedMeshData& MeshData,
        UMaterialInterface* ParentMaterialInterface, AActor* const Owner,
        const bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	return ConstructMeshComponentFromMeshData<UProceduralMeshComponent>(
	    MeshData, ParentMaterialInterface, Owner, ShouldRegisterComponentToOwner);
}

UStaticMeshComponent*
    UAssetConstructor::ConstructStaticMeshComponentFromMeshData(
        const FLoadedMeshData& MeshData,
        UMaterialInterface* ParentMaterialInterface, AActor* const Owner,
        const bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	return ConstructMeshComponentFromMeshData<UStaticMeshComponent>(
	    MeshData, ParentMaterialInterface, Owner, ShouldRegisterComponentToOwner);
}

UDynamicMeshComponent*
    UAssetConstructor::ConstructDynamicMeshComponentFromMeshData(
        const FLoadedMeshData& MeshData,
        UMaterialInterface* ParentMaterialInterface, AActor* const Owner,
        const bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	return ConstructMeshComponentFromMeshData<UDynamicMeshComponent>(
	    MeshData, ParentMaterialInterface, Owner, ShouldRegisterComponentToOwner);
}

UProceduralMeshComponent*
    UAssetConstructor::ConstructProceduralMeshComponentFromAssetFile(
        const FString&            FilePath,
        UMaterialInterface* const ParentMaterialInterface, AActor* const Owner,
        EConstructProceduralMeshComponentFromAssetFileResult&
                   ConstructProceduralMeshComponentFromAssetFileResult,
        const bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// load mesh from asset file(path: FilePath)
	ELoadMeshFromAssetFileResult LoadMeshFromAssetFileResult;
	const auto& LoadedMeshData = UAssetLoader::LoadMeshFromAssetFile(
	    FilePath, LoadMeshFromAssetFileResult);

	// check load result
	if (ELoadMeshFromAssetFileResult::Failure == LoadMeshFromAssetFileResult) {
		ConstructProceduralMeshComponentFromAssetFileResult =
		    EConstructProceduralMeshComponentFromAssetFileResult::Failure;
	}
	check(ELoadMeshFromAssetFileResult::Success == LoadMeshFromAssetFileResult);

	// assume the result is success
	ConstructProceduralMeshComponentFromAssetFileResult =
	    EConstructProceduralMeshComponentFromAssetFileResult::Success;

	// construct from loaded mesh data
	return ConstructProceduralMeshComponentFromMeshData(
	    LoadedMeshData, ParentMaterialInterface, Owner,
	    ShouldRegisterComponentToOwner);
}

UStaticMeshComponent*
    UAssetConstructor::ConstructStaticMeshComponentFromAssetFile(
        const FString& FilePath, UMaterialInterface* ParentMaterialInterface,
        AActor* Owner,
        EConstructStaticMeshComponentFromAssetFileResult&
             ConstructStaticMeshComponentFromAssetFileResult,
        bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// load mesh from asset file(path: FilePath)
	ELoadMeshFromAssetFileResult LoadMeshFromAssetFileResult;
	const auto& LoadedMeshData = UAssetLoader::LoadMeshFromAssetFile(
	    FilePath, LoadMeshFromAssetFileResult);

	// check load result
	if (ELoadMeshFromAssetFileResult::Failure == LoadMeshFromAssetFileResult) {
		ConstructStaticMeshComponentFromAssetFileResult =
		    EConstructStaticMeshComponentFromAssetFileResult::Failure;
	}
	check(ELoadMeshFromAssetFileResult::Success == LoadMeshFromAssetFileResult);

	// assume the result is success
	ConstructStaticMeshComponentFromAssetFileResult =
	    EConstructStaticMeshComponentFromAssetFileResult::Success;

	// construct from loaded mesh data
	return ConstructStaticMeshComponentFromMeshData(
	    LoadedMeshData, ParentMaterialInterface, Owner,
	    ShouldRegisterComponentToOwner);
}

UDynamicMeshComponent*
    UAssetConstructor::ConstructDynamicMeshComponentFromAssetFile(
        const FString& FilePath, UMaterialInterface* ParentMaterialInterface,
        AActor* Owner,
        EConstructDynamicMeshComponentFromAssetFileResult&
             ConstructDynamicMeshComponentFromAssetFileResult,
        bool ShouldRegisterComponentToOwner) {
	// check to ParentMaterialInterface is properly set
	check(ParentMaterialInterface != nullptr);

	// check to Owner is properly set
	check(Owner != nullptr);

	// load mesh from asset file(path: FilePath)
	ELoadMeshFromAssetFileResult LoadMeshFromAssetFileResult;
	const auto& LoadedMeshData = UAssetLoader::LoadMeshFromAssetFile(
	    FilePath, LoadMeshFromAssetFileResult);

	// check load result
	if (ELoadMeshFromAssetFileResult::Failure == LoadMeshFromAssetFileResult) {
		ConstructDynamicMeshComponentFromAssetFileResult =
		    EConstructDynamicMeshComponentFromAssetFileResult::Failure;
	}
	check(ELoadMeshFromAssetFileResult::Success == LoadMeshFromAssetFileResult);

	// assume the result is success
	ConstructDynamicMeshComponentFromAssetFileResult =
	    EConstructDynamicMeshComponentFromAssetFileResult::Success;

	// construct from loaded mesh data
	return ConstructDynamicMeshComponentFromMeshData(
	    LoadedMeshData, ParentMaterialInterface, Owner,
	    ShouldRegisterComponentToOwner);
}

#pragma region                           definitions of static functions
static TArray<UMaterialInstanceDynamic*> GenerateMaterialInstances(
    UObject& Owner, const TArray<FLoadedMaterialData>& MaterialDataList,
    UMaterialInterface& ParentMaterialInterface) {
	TArray<UMaterialInstanceDynamic*> MaterialInstances;
	const auto&                       NumMaterials = MaterialDataList.Num();
	MaterialInstances.AddUninitialized(NumMaterials);

	if (0 == NumMaterials) {
		UE_LOG(LogAssetConstructor, Display, TEXT("There is no Materials."));
	}
	for (auto i = decltype(NumMaterials){0}; i < NumMaterials; ++i) {
		// create material
		UMaterialInstanceDynamic* MaterialInstance =
		    UMaterialInstanceDynamic::Create(&ParentMaterialInterface, &Owner);

		const auto& MaterialData = MaterialDataList[i];

		switch (MaterialData.ColorStatus) {
		case EColorStatus::None:
			// if nothing is set
			UE_LOG(LogAssetConstructor, Error,
			       TEXT("color status is not set in index %d"), i);

			break;
		case EColorStatus::ColorIsSet: {
			// log that no texture is found
			UE_LOG(LogAssetConstructor, Log,
			       TEXT("No texture is found for material in index %d"), i);

			VerifyMaterialParameter(ParentMaterialInterface,
			                        EMaterialParameterType::Scalar,
			                        "TextureBlendIntensityForBaseColor");
			VerifyMaterialParameter(ParentMaterialInterface,
			                        EMaterialParameterType::Vector, "BaseColor4");

			// set to use color
			MaterialInstance->SetScalarParameterValue(
			    "TextureBlendIntensityForBaseColor", 0.0f);

			// get color
			const auto& Color = MaterialData.Color;

			// set color
			MaterialInstance->SetVectorParameterValue("BaseColor4", Color);

			break;
		}
		case EColorStatus::TextureIsSet: {
			// get compressed texture data
			const auto& CompressedTextureData = MaterialData.CompressedTextureData;

			// get texture
			UTexture2D* Texture0 =
			    FImageUtils::ImportBufferAsTexture2D(CompressedTextureData);

			VerifyMaterialParameter(ParentMaterialInterface,
			                        EMaterialParameterType::Scalar,
			                        "TextureBlendIntensityForBaseColor");
			VerifyMaterialParameter(ParentMaterialInterface,
			                        EMaterialParameterType::Texture,
			                        "BaseColorTexture");

			// set to use texture
			MaterialInstance->SetScalarParameterValue(
			    "TextureBlendIntensityForBaseColor", 1.0f);

			// set texture
			MaterialInstance->SetTextureParameterValue("BaseColorTexture", Texture0);

			break;
		}
		case EColorStatus::TextureWasSetButError:
			// if the texture is in the status of failure
			UE_LOG(LogAssetConstructor, Warning,
			       TEXT("The original data had a texture set, but it failed to load, "
			            "so skip setting the texture in index %d"),
			       i);

			break;
		default:
			verifyf(false, TEXT("Bug. Color status is not None, ColorIsSet, "
			                    "TextureIsSet, or TextureWasSetButError."));
			break;
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
#pragma endregion
