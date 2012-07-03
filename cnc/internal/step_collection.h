#ifndef _QT_CNC_STEP_COLLECTION_H_
#define _QT_CNC_STEP_COLLECTION_H_

namespace CnC {
	template< typename StepType >
	template< typename ContextTemplate >step_collection< StepType >::step_collection(context< ContextTemplate > &ctxt) : _step(StepType()) {}

	template< typename StepType >
	StepType step_collection< StepType >:: get_step() const
	{
		return _step;
	}
} // namespace CnC

#endif // _QT_CNC_STEP_COLLECTION_H_
