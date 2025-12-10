// Fill out your copyright notice in the Description page of Project Settings.


#include "PannerMetasoundNode.h"

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundAudioBuffer.h"
#include "DSP/Dsp.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PannerMetasoundPlugin"

namespace Metasound
{
	namespace PannerVertexNames
	{
		//Usual Metasound Pin name and Metadata
		METASOUND_PARAM(InputAudio, "Audio In", "Mono input signal");
		METASOUND_PARAM(PanAmount, "Pan Amount", "-1..1: -1 will be full left, 0 = center, 1 = full right");
		METASOUND_PARAM(PanLaw, "Panning Law", "0 = Equal Power, 1 = Linear Power");
		
		METASOUND_PARAM(OutputLeft, "Left", "Left output channel");
		METASOUND_PARAM(OutputRight, "Right", "Right output channel");
	}
	
	enum class EPanLaw
	{
		EqualPowerPower = 0,
		LinearLi,
	};
	
	DECLARE_METASOUND_ENUM(
	   EPanLaw,                    
	   EPanLaw::EqualPowerPower,        
	   LEARNING_METASOUNDPLUGIN_API, 
	   FEnumPanLaw,                
	   FEnumPanLawInfo,            
	   FEnumPanLawReadRef,         
	   FEnumPanLawWriteRef         
   );
	
	DEFINE_METASOUND_ENUM_BEGIN(EPanLaw, FEnumPanLaw, "PanLaw")
		DEFINE_METASOUND_ENUM_ENTRY(EPanLaw::EqualPowerPower,
			"PanningLawEqualPowerName", "Equal Power", "PanningLawEqualPowerTT", "Equal power panning (default)"),
		DEFINE_METASOUND_ENUM_ENTRY(EPanLaw::LinearLi,
			"PanningLawLinearName", "Linear", "PanningLawLinearTT", "The amplitude of the audio signal is constant while panning."),
	DEFINE_METASOUND_ENUM_END()
	
	
	class FPannerPanOperator : public TExecutableOperator<FPannerPanOperator>
	{
	public:
		
		FPannerPanOperator(
			const FBuildOperatorParams& InParams,
			const FAudioBufferReadRef& InAudioInput,
			const FFloatReadRef& InPanningAmount,
			const FEnumPanLawReadRef& InPanningLaw)
			: AudioInput(InAudioInput)
			, PanningAmount(InPanningAmount)
			, PanningLaw(InPanningLaw)
			, AudioLeft(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings))
			, AudioRight(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings))
		{
			Reset(InParams);
		}
		
		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FVertexInterface NodeInterface = DeclareVertexInterface();
				FNodeClassMetadata Metadata
				{
					FNodeClassName { StandardNodes::Namespace, "PanPan", StandardNodes::AudioVariant},
					1,
					0,
					METASOUND_LOCTEXT("PanPan", "Auto Pan"),
					METASOUND_LOCTEXT("PanPan desc", "Simple  panner"),
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{ NodeCategories::Spatialization },
					{},
					FNodeDisplayStyle()
				};
				return Metadata;
			};
			
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace PannerVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(PanAmount), PanningAmount);
			/*InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(PanLaw), PanningLaw);*/
		}
		
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace PannerVertexNames;
			
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputLeft), AudioLeft);
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputRight), AudioRight);
		}
		
		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace PannerVertexNames;
			
			static const FVertexInterface Interface(
				FInputVertexInterface(
						TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)),
						TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(PanAmount)),
						TInputDataVertex<FEnumPanLaw>(METASOUND_GET_PARAM_NAME_AND_METADATA(PanLaw), (int32)EPanLaw::EqualPowerPower)
						),
					FOutputVertexInterface(
						TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputLeft)),
						TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRight)))
				);
			return Interface;
		}
		
		
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			
			using namespace PannerVertexNames;
			
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			
			FAudioBufferReadRef AudioIn = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
				METASOUND_GET_PARAM_NAME(InputAudio),
				InParams.OperatorSettings);
			
			FFloatReadRef PanRateIn = InputData.GetOrCreateDefaultDataReadReference<float>(
				METASOUND_GET_PARAM_NAME(PanAmount),
				InParams.OperatorSettings);
			
			FEnumPanLawReadRef PanningLawIn = InputData.GetOrCreateDefaultDataReadReference<FEnumPanLaw>(
				METASOUND_GET_PARAM_NAME(PanLaw),
				InParams.OperatorSettings);
			
			return MakeUnique<FPannerPanOperator>(InParams, AudioIn, PanRateIn, PanningLawIn);
		}
		
		void Reset(const IOperator::FResetParams& InParams)
		{
			Phase = 0.f;
			AudioLeft->Zero();
			AudioRight->Zero();
		}
		
		//DSP Calculation
		void Execute()
		{
			const int32 NumFrames = AudioInput->Num();
			
			//If our buffer don't match size we return
			if (AudioLeft->Num() != NumFrames || AudioRight->Num() != NumFrames)
			{
				return;
			}
			
			//Clearing Block Samples
			AudioLeft->Zero();
			AudioRight->Zero();
			
			//Here I am getting the Raw pointers to the audio data here
			const float* InData   = AudioInput->GetData();
			float* LeftData       = AudioLeft->GetData();
			float* RightData      = AudioRight->GetData();
			
			//Simple Clamp to get parameters to the desirable value
			float Pan = FMath::Clamp(*PanningAmount, -1.f, 1.f);
			
			//Remap
			const float t = 0.5f * (Pan + 1.f);
			
			//Get The PanningLaw
			const bool bEqualPower = (*PanningLaw == EPanLaw::EqualPowerPower);
			
			float LeftGain{1.f};
			float RightGain{1.f};
			
			if (bEqualPower)
			{
				LeftGain = FMath::Cos(t * HALF_PI);
				RightGain = FMath::Sin(t * HALF_PI);
			}
			else
			{
				LeftGain = 1.f - t;
				RightGain = t;
			}
			
			//Check UnrealMathUtility.h for Macros related to Math
			//DSP Loop
			for (int32 i = 0; i < NumFrames; i++)
			{
				const float InSample = InData[i];
				LeftData[i] = LeftGain * InSample;
				RightData[i] = RightGain * InSample;
			}
			
		}
		
	private:
		FAudioBufferReadRef  AudioInput;
		
		FFloatReadRef PanningAmount;
		
		FEnumPanLawReadRef PanningLaw;
		
		FAudioBufferWriteRef AudioLeft;
		FAudioBufferWriteRef AudioRight;
		
		float PrevPanningAmount{0.f};
		float PrevLeftPan{0.f};
		float PrevRightPan{0.f};
		
		float SampleRate;
		float Phase;
		
	};
	
	class FPannerPanNode : public FNodeFacade
	{
	public:
		FPannerPanNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FPannerPanOperator>())
		{}
	};
	
	METASOUND_REGISTER_NODE(FPannerPanNode);
	
}


PannerMetasoundNode::PannerMetasoundNode()
{
}

PannerMetasoundNode::~PannerMetasoundNode()
{
}
