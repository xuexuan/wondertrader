#include "ParserAdapter.h"
#include "DataManager.h"
#include "StateMonitor.h"

#include "../Share/TimeUtils.hpp"
#include "../Share/WTSParams.hpp"
#include "../Share/WTSContractInfo.hpp"
#include "../Share/WTSDataDef.hpp"

#include "../WTSTools/WTSBaseDataMgr.h"
#include "../WTSTools/WTSLogger.h"


//////////////////////////////////////////////////////////////////////////
//ParserAdapter
ParserAdapter::ParserAdapter(WTSBaseDataMgr * bgMgr, DataManager* dtMgr)
	: m_pParser(NULL)
	, m_funcCreate(NULL)
	, m_funcDelete(NULL)
	, m_bStopped(false)
	, m_bdMgr(bgMgr)
	, m_dtMgr(dtMgr)
{
}


ParserAdapter::~ParserAdapter()
{
}

bool ParserAdapter::isExchgValid(const char* exchg)
{
	if (!m_exchgFilter.empty() && (m_exchgFilter.find(exchg) == m_exchgFilter.end()))
		return false;

	return true;
}

bool ParserAdapter::isCodeValid(const char* code)
{
	if (!m_codeFilter.empty() && (m_codeFilter.find(code) == m_codeFilter.end()))
		return false;

	return true;
}

bool ParserAdapter::initAdapter(WTSParams* params, FuncCreateParser funcCreate, FuncDeleteParser funcDelete)
{
	m_funcCreate = funcCreate;
	m_funcDelete = funcDelete;

	const std::string& strFilter = params->getString("filter");
	if(!strFilter.empty())
	{
		const StringVector &ayFilter = StrUtil::split(strFilter, ",");
		auto it = ayFilter.begin();
		for(; it != ayFilter.end(); it++)
		{
			m_exchgFilter.insert(*it);
		}
	}
	
	std::string& strCodes = params->getString("code");
	if (!strCodes.empty())
	{
		const StringVector &ayCodes = StrUtil::split(strCodes, ",");
		auto it = ayCodes.begin();
		for (; it != ayCodes.end(); it++)
		{
			m_codeFilter.insert(*it);
		}
	}


	m_pParser = m_funcCreate();
	if(m_pParser)
	{
		m_pParser->registerListener(this);

		if(m_pParser->init(params))
		{
			ContractSet contractSet;
			if (!m_codeFilter.empty())//�����жϺ�Լ������
			{
				ExchgFilter::iterator it = m_codeFilter.begin();
				for (; it != m_codeFilter.end(); it++)
				{
					WTSContractInfo* contract = m_bdMgr->getContract((*it).c_str());
					WTSCommodityInfo* pCommInfo = m_bdMgr->getCommodity(contract);
					if (pCommInfo->getCategoty() == CC_Future || pCommInfo->getCategoty() == CC_Option || pCommInfo->getCategoty() == CC_Stock)
					{
						contractSet.insert(contract->getCode());
					}
				}
			}
			else if(!m_exchgFilter.empty())//���жϽ�����������
			{
				ExchgFilter::iterator it = m_exchgFilter.begin();
				for(; it != m_exchgFilter.end(); it++)
				{
					WTSArray* ayContract = m_bdMgr->getContracts((*it).c_str());
					WTSArray::Iterator it = ayContract->begin();
					for(; it != ayContract->end(); it++)
					{
						WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
						WTSCommodityInfo* pCommInfo = m_bdMgr->getCommodity(contract);
						if (pCommInfo->getCategoty() == CC_Future || pCommInfo->getCategoty() == CC_Option || pCommInfo->getCategoty() == CC_Stock)
						{
							contractSet.insert(contract->getCode());
						}
					}

					ayContract->release();
				}
			}
			else//������ȫ����
			{
				WTSArray* ayContract = m_bdMgr->getContracts();
				WTSArray::Iterator it = ayContract->begin();
				for(; it != ayContract->end(); it++)
				{
					WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
					WTSCommodityInfo* pCommInfo = m_bdMgr->getCommodity(contract);
					if (pCommInfo->getCategoty() == CC_Future || pCommInfo->getCategoty() == CC_Option || pCommInfo->getCategoty() == CC_Stock)
					{
						contractSet.insert(contract->getCode());
					}
				}

				ayContract->release();
			}
			m_pParser->subscribe(contractSet);
			m_pParser->connect();
			contractSet.clear();			
		}
		else
		{
			WTSLogger::info("����ģ���ʼ��ʧ��,ģ��ӿڳ�ʼ��ʧ��...");
		}
	}
	else
	{
		WTSLogger::info("����ģ���ʼ��ʧ��,��ȡģ��ӿ�ʧ��...");
	}

	return true;
}

void ParserAdapter::release()
{
	m_bStopped = true;
	if (m_pParser)
	{
		m_pParser->release();
	}

	m_funcDelete(m_pParser);
}

void ParserAdapter::handleSymbolList( const WTSArray* aySymbols )
{
	
}

void ParserAdapter::handleTransaction(WTSTransData* transData)
{
	if (m_bStopped)
		return;

	if (!isExchgValid(transData->exchg()) || !isCodeValid(transData->code()))
		return;

	if (transData->actiondate() == 0 || transData->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(transData->code());
	if (contract == NULL)
		return;

	WTSCommodityInfo* commInfo = m_bdMgr->getCommodity(contract);
	if (commInfo == NULL)
		return;


	m_dtMgr->writeTransaction(transData);
}

void ParserAdapter::handleOrderDetail(WTSOrdDtlData* ordDetailData)
{
	if (m_bStopped)
		return;

	if (!isExchgValid(ordDetailData->exchg()) || !isCodeValid(ordDetailData->code()))
		return;

	if (ordDetailData->actiondate() == 0 || ordDetailData->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(ordDetailData->code());
	if (contract == NULL)
		return;

	WTSCommodityInfo* commInfo = m_bdMgr->getCommodity(contract);
	if (commInfo == NULL)
		return;


	m_dtMgr->writeOrderDetail(ordDetailData);
}

void ParserAdapter::handleOrderQueue(WTSOrdQueData* ordQueData)
{
	if (m_bStopped)
		return;

	if (!isExchgValid(ordQueData->exchg()) || !isCodeValid(ordQueData->code()))
		return;

	if (ordQueData->actiondate() == 0 || ordQueData->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(ordQueData->code());
	if (contract == NULL)
		return;

	WTSCommodityInfo* commInfo = m_bdMgr->getCommodity(contract);
	if (commInfo == NULL)
		return;


	m_dtMgr->writeOrderQueue(ordQueData);
}

void ParserAdapter::handleQuote( WTSTickData *quote, bool bPreProc )
{
	if (m_bStopped)
		return;

	if (!isExchgValid(quote->exchg()) || !isCodeValid(quote->code()))
		return;

	if (quote->actiondate() == 0 || quote->tradingdate() == 0)
		return;

	WTSContractInfo* contract = m_bdMgr->getContract(quote->code());
	if (contract == NULL)
		return;

	WTSCommodityInfo* commInfo = m_bdMgr->getCommodity(contract);
	if (commInfo == NULL)
		return;


	if (!m_dtMgr->writeTick(quote, bPreProc))
		return;
}

void ParserAdapter::handleParserLog( WTSLogLevel ll, const char* format, ... )
{
	if (m_bStopped)
		return;

	char szBuf[2048] = {0};
	va_list args;
	va_start(args, format);        
	vsprintf(szBuf, format, args);
	va_end(args);

	WTSLogger::log2("parser", ll, szBuf);
}

IBaseDataMgr* ParserAdapter::getBaseDataMgr()
{
	return m_bdMgr;
}


//////////////////////////////////////////////////////////////////////////
//ParserAdapterMgr
ParserAdapterVec ParserAdapterMgr::m_ayAdapters;

void ParserAdapterMgr::releaseAdapters()
{
	ParserAdapterVec::iterator it = m_ayAdapters.begin();
	for(; it != m_ayAdapters.end(); it++)
	{
		(*it)->release();
	}

	m_ayAdapters.clear();
}

void ParserAdapterMgr::addAdapter(ParserAdapterPtr& adapter)
{
	m_ayAdapters.push_back(adapter);
}