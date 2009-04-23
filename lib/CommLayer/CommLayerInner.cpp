#include "CommLayerInner.h"

CCommLayerInner::CCommLayerInner(CObserver *pObs)
: DEBUGINFO(pObs),
  WARNING(pObs),
  STATUS(pObs)
{
    m_pObs = pObs;
}

CCommLayerInner::~CCommLayerInner()
{
}

CDebug& CCommLayerInner::Debug()
{
    return DEBUGINFO;
}

CWarning& CCommLayerInner::Warning()
{
    return WARNING;
}

CStatusUpdate& CCommLayerInner::Status()
{
    return STATUS;
}
