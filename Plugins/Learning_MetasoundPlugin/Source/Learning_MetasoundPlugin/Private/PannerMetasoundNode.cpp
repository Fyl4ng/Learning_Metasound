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
		METASOUND_PARAM(PanRate, "Pan Rate", "How many L-R cycles per second.");
		METASOUND_PARAM(PanDepth, "Pan Depth", "0..1: how far to pan left/right");
		
		METASOUND_PARAM(OutputLeft, "Left", "Left output channel");
		METASOUND_PARAM(OutputRight, "Right", "Right output channel");
	}
	
	class FPannerPanOperator : public TExecutableOperator<FPannerPanOperator>
	{
	public:
		
		FPannerPanOperator(
			const FBuildOperatorParams& InParams,
			const FAudioBufferReadRef&  InAudioBuffer,
			const FFloatReadRef&		InPanRate,
			const FFloatReadRef&		InPanDepth) :
			AudioInput(InAudioBuffer),
			PanRate(InPanRate),
			PanDepth(InPanDepth),
			AudioLeft(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings)),
			AudioRight(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings)),
			SampleRate(InParams.OperatorSettings.GetSampleRate()),
			Phase(0.f)
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
					FNodeClassName { StandardNodes::Namespace, "Panner", StandardNodes::AudioVariant},
					1,
					0,
					METASOUND_LOCTEXT("PannerDisplayName", "Auto Pan"),
					METASOUND_LOCTEXT("Panner desc", "Simple mono-to-stereo auto panner"),
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
		
		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace PannerVertexNames;
			
			static const FVertexInterface Interface(
				FInputVertexInterface(
						TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)),
						TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(PanRate)),
						TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(PanDepth))
						),
					FOutputVertexInterface(
						TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputLeft)),
						TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRight)))
				);
			return Interface;
		}
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace PannerVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(PanRate), PanRate);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(PanDepth), PanDepth);
		}
		
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace PannerVertexNames;
			
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputLeft), AudioLeft);
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputRight), AudioRight);
		}
		
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			
			using namespace PannerVertexNames;
			
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			
			FAudioBufferReadRef AudioIn = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
				METASOUND_GET_PARAM_NAME(InputAudio),
				InParams.OperatorSettings);
			
			FFloatReadRef PanRateIn = InputData.GetOrCreateDefaultDataReadReference<float>(
				METASOUND_GET_PARAM_NAME(PanRate),
				InParams.OperatorSettings);
			
			FFloatReadRef PanDepthIn = InputData.GetOrCreateDefaultDataReadReference<float>(
				METASOUND_GET_PARAM_NAME(PanDepth),
				InParams.OperatorSettings);
			
			return MakeUnique<FPannerPanOperator>(InParams, AudioIn, PanRateIn, PanDepthIn);
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
			const float Rate = FMath::Max(0.f, *PanRate);
			const float Depth = FMath::Clamp(*PanDepth, 0.f, 1.f);
			
			const float Result = Rate / SampleRate;
			
			//Check UnrealMathUtility.h for Macros related to Math
			//DSP Loop
			for (int32 i = 0; i < NumFrames; i++)
			{
				//Using a LFO to generate a sin wave modulation
				//I will use the LFO to move the stereo gain left - right
				const float PanLfo = FMath::Sin(Phase);
				
				//Phase is the angle in radians that will be used inside sin()
				//One cycle full cycle around the circle will be 2PI radians/
				//Here I am using this formula to make the phase complete f cycles per second
				//For example if I want 440hz I would need 440 cycles or the sound wont have the right pitch
				//If DSP Oscillators dont match the definition of frequency they will produce the wrong note.
				Phase += TWO_PI * Result;
				
				//2PI angle will be the same as Phase 0 sooo I am keeping the wave stable by checking it
				if (Phase > TWO_PI)
				{
					Phase -= TWO_PI;
				}
				
				//Here PanLfo is the movement and Depth is the range of the momvent
				//Depth will control how much of this LFO I actually want to use
				const float Pan = PanLfo * Depth;
				
				//Remapping value from 0 to 1 because the Equal-power formula expects it
				const float t = 0.5f * (Pan + 1.f);
				
				//Equal-Power Formula
				//When T 0 = full left
				//When T 0.5 = center
				//When T 1 - Full Right]
				//Using HALF_PI here because of geometry of panning, I want to visualize the curve as 90 degres of the circle not 180
				const float LeftGain = FMath::Cos(t * HALF_PI);
				const float RightGain = FMath::Sin(t * HALF_PI);
				
				//Mono Input Sample
				const float InSample = InData[i];
				
				//Feeding data for the Outputs
				LeftData[i] = InSample * LeftGain;
				RightData[i] = InSample * RightGain;
				
			}
			
		}
		
	private:
		FAudioBufferReadRef  AudioInput;
		FFloatReadRef        PanRate;
		FFloatReadRef        PanDepth;

		FAudioBufferWriteRef AudioLeft;
		FAudioBufferWriteRef AudioRight;

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
