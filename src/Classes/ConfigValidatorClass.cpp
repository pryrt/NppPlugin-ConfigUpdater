#include "ConfigValidatorClass.h"
#include "ValidateXML.h"

ConfigValidator::ConfigValidator(HWND hwndNpp, HWND hwndFileCbx, HWND hwndErrLbx)
{
	_hw.npp = hwndNpp;
	_hw.fileCbx = hwndFileCbx;
	_hw.errLbx = hwndErrLbx;
}
