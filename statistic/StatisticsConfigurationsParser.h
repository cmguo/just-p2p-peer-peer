//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_CONFIGURATIONS_PARSER_H_
#define _STATISTIC_STATISTICS_CONFIGURATIONS_PARSER_H_

#include "tinyxml.h"

namespace statistic
{
    struct StatisticsConfigurations;
    struct StatisticsConfiguration;
    class CollectionCriteria;

    class IStatisticsConfigurationsParser
    {
    public:
        virtual boost::shared_ptr<StatisticsConfigurations> Parse(const string& config_xml) = 0;
        virtual ~IStatisticsConfigurationsParser(){}
    };

    class StatisticsConfigurationsParser: public IStatisticsConfigurationsParser
    {
    private:
        class Attributes
        {
        public:
            static const string ExpiresInMinutes;
            static const string Id;
            static const string ReportingProbability;
            static const string CriteriaType;
        };

        class Elements
        {
        public:
            static const string StatisticsConfigurations;

            static const string StatisticsConfiguration;
            static const string CollectionCriteria;
            static const string CollectionLength;
            static const string MaxReportingOccurrence;
            static const string Resources;
            static const string RID;
        };
    public:
        class DefaultValues
        {
        public:
            static const int ExpiresInMinutes;
        };

    public:
        boost::shared_ptr<StatisticsConfigurations> Parse(const string& config_xml);

    private:
        static boost::shared_ptr<StatisticsConfigurations> ParseXml(const tinyxml::TiXmlDocument& document);
        static bool TryParseAsStatisticsConfiguration(const tinyxml::TiXmlElement* config_element, StatisticsConfiguration& statistics_config);
        static void SetDefaultValues(StatisticsConfigurations& config);
        static const char* GetChildElementText(const tinyxml::TiXmlElement* parent_element, string child_element_name);
        static bool IsKnownStatisticsConfigurationChildElement(const string& element_name);
        static void ParseAsCriteria(const tinyxml::TiXmlElement* criteria_element, CollectionCriteria& criteria);
    };
}

#endif  // _STATISTIC_STATISTICS_CONFIGURATIONS_PARSER_H_
