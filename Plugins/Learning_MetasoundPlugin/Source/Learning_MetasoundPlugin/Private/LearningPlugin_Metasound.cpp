// Fill out your copyright notice in the Description page of Project Settings.


#include "LearningPlugin_Metasound.h"
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

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_LearningPlugin_Metasound"


namespace Metasound
{

	namespace LearningVertexNames
	{
		METASOUND_PARAM(InputAudio, "Audio", "Copy from bitcrusher")
		METASOUND_PARAM(OutputAudio, "Audio", "Copy from bitcrusher")
	}
	
	class FLearningOperator : public TExecutableOperator<FLearningOperator>
	{
	public:
		
		FLearningOperator(const FBuildOperatorParams& InParams,
			const FAudioBufferReadRef& InAudioBuffer)
			: AudioInput(InAudioBuffer)
			, AudioOutput(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings))
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
					FNodeClassName { StandardNodes::Namespace, "LearningTest", StandardNodes::AudioVariant },
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("LearningTestDisplayName", "LearningTest"),
					METASOUND_LOCTEXT("LearningTestDesc", "a"),
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{ NodeCategories::Filters },
					{ },
					FNodeDisplayStyle()
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}
		
		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace LearningVertexNames;
			
			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio))	
				),
				FOutputVertexInterface(
					    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
					)
				);
			return Interface;
		}
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace LearningVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
		}
		
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace LearningVertexNames;
			
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);
			
		}
		
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace LearningVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			
			FAudioBufferReadRef AudioIn = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudio), InParams.OperatorSettings);
			
			return MakeUnique<FLearningOperator>(InParams, AudioIn);
		}
		
		void Reset(const IOperator::FResetParams& InParams)
		{
			AudioOutput->Zero();
			
		}
		
		void Execute()
		{
			const int32 NumFrames = AudioInput->Num();
			
			if (NumFrames != AudioOutput->Num())
			{
				return;
			}
		}
		
	private:
		FAudioBufferReadRef AudioInput;
		FAudioBufferWriteRef AudioOutput;
	};
	
	class FLearningNode : public FNodeFacade
	{
	public:
		FLearningNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FLearningOperator>())
		{}
	};
	
	METASOUND_REGISTER_NODE(FLearningNode)
	
}

#undef LOCTEXT_NAMESPACE

LearningPlugin_Metasound::LearningPlugin_Metasound()
{
}

LearningPlugin_Metasound::~LearningPlugin_Metasound()
{
}
