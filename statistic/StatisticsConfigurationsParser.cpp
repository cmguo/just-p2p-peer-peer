//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsConfigurationsParser.h"
#include "statistic/StatisticsConfiguration.h"
#include "statistic/StatisticsReportingConfiguration.h"
#include "statistic/StatisticsCollectorSettings.h"

#include <boost/algorithm/string.hpp>

namespace statistic
{
    using namespace tinyxml;

    //constants
    const string StatisticsConfigurationsParser::Attributes::ExpiresInMinutes = "expiresInMinutes";
    const string StatisticsConfigurationsParser::Attributes::Id = "id";
    const string StatisticsConfigurationsParser::Attributes::ReportingProbability = "reportingProbability";
    const string StatisticsConfigurationsParser::Attributes::CriteriaType = "type";

    const string StatisticsConfigurationsParser::Elements::StatisticsConfigurations = "StatisticsConfigurations";
    const string StatisticsConfigurationsParser::Elements::StatisticsConfiguration = "StatisticsConfiguration";
    const string StatisticsConfigurationsParser::Elements::CollectionCriteria = "CollectionCriteria";
    const string StatisticsConfigurationsParser::Elements::CollectionLength = "CollectionLength";
    const string StatisticsConfigurationsParser::Elements::MaxReportingOccurrence = "MaxReportingOccurrence";
    const string StatisticsConfigurationsParser::Elements::Resources = "Resources";
    const string StatisticsConfigurationsParser::Elements::RID = "RID";

    const int StatisticsConfigurationsParser::DefaultValues::ExpiresInMinutes = 60*24*3;

    boost::shared_ptr<StatisticsConfigurations> StatisticsConfigurationsParser::Parse(const string& config_xml)
    {
        boost::shared_ptr<StatisticsConfigurations> config;

        TiXmlDocument document;
        document.Parse(config_xml.c_str());
        if (false == document.Error())
        {
            config = ParseXml(document);
        }
        
        return config;
    }

    /*
    <StatisticsConfigrations expiresInMinutes="1440">
        <StatisticsConfiguration id="vod-bufferring-basic" reportingProbability="2">
            ...
        </StatisticsConfiguration>
    </StatisticsConfigrations>
    */

    boost::shared_ptr<StatisticsConfigurations> StatisticsConfigurationsParser::ParseXml(
        const tinyxml::TiXmlDocument& document)
    {
        boost::shared_ptr<StatisticsConfigurations> config;

        const TiXmlElement* root_config_element = document.RootElement();
        if (!root_config_element)
        {
            return config;
        }

        if (root_config_element->ValueStr() != Elements::StatisticsConfigurations)
        {
            return config;
        }

        config.reset(new StatisticsConfigurations());

        root_config_element->Attribute(Attributes::ExpiresInMinutes.c_str(), &config->expires_in_minutes_);

        for(const TiXmlElement * config_element = root_config_element->FirstChildElement(); 
            config_element != 0;
            config_element = config_element->NextSiblingElement())
        {
            StatisticsConfiguration child_config;
            if (TryParseAsStatisticsConfiguration(config_element, child_config))
            {
                config->statistics_configurations_.push_back(child_config);
            }
            else
            {
                //should we ignore any error?
            }
        }

        SetDefaultValues(*config);

        return config;            
    }

    /*
    a typical StatisticsConfiguration:
    <StatisticsConfiguration id="vod-bufferring-basic" reportingProbability="2">
        <CollectionCriteria type="VOD-Bufferring"/>
        <CollectionLength>15</CollectionLength>
        <MaxReportingOccurrence>1</MaxReportingOccurrence>
    </StatisticsConfiguration>
    */
    bool StatisticsConfigurationsParser::TryParseAsStatisticsConfiguration(
        const tinyxml::TiXmlElement* config_element, 
        StatisticsConfiguration& statistics_config)
    {
        if (config_element->ValueStr() != Elements::StatisticsConfiguration)
        {
            return false;
        }

        const string* id_value = config_element->Attribute(Attributes::Id);
        if (id_value)
        {
            statistics_config.id_ = *id_value;
        }
        
        config_element->Attribute(Attributes::ReportingProbability, &statistics_config.reporting_probability_);

        const TiXmlElement* criteria_element = config_element->FirstChildElement(Elements::CollectionCriteria);
        if (criteria_element)
        {
            ParseAsCriteria(criteria_element, statistics_config.criteria_);
        }

        //optional children elements
        const char* collection_length = GetChildElementText(config_element, Elements::CollectionLength);
        if (collection_length)
        {
            statistics_config.collection_length_ = atoi(collection_length);
        }

        const char* max_reporting_occurrence = GetChildElementText(config_element, Elements::MaxReportingOccurrence);
        if (max_reporting_occurrence)
        {
            statistics_config.max_reporting_occurrences_ = atoi(max_reporting_occurrence);
        }

        //populate other settings, if any
        for(const TiXmlElement* child_element = config_element->FirstChildElement();
            child_element != 0;
            child_element = child_element->NextSiblingElement())
        {
            if (!IsKnownStatisticsConfigurationChildElement(child_element->ValueStr()))
            {
                statistics_config.misc_settings_[child_element->ValueStr()] = child_element->GetText();
            }
        }

        return true;
    }

    /*
        CollectionCriteria element could be as simple as:
        <CollectionCriteria type="VOD-Bufferring"/>

        or more complicated as:

        <CollectionCriteria type="LIVE-Bufferring">
            <Resources>
                <RID>LIVE-Channel1</RID>
                <RID>LIVE-Channel2</RID>
            </Resources>
        </CollectionCriteria>
    */
    void StatisticsConfigurationsParser::ParseAsCriteria(const TiXmlElement* criteria_element, CollectionCriteria& criteria)
    {
        assert(criteria_element);
        const string* criteria_type = criteria_element->Attribute(Attributes::CriteriaType);
        if(criteria_type)
        {
            string upper_case_value = *criteria_type;
            boost::algorithm::to_upper(upper_case_value);
            if (upper_case_value == "LIVE-BUFFERRING")
            {
                criteria.condition_type_ = Bufferring;
                criteria.target_resource_type_ = Live;
            }
            else if (upper_case_value == "VOD-BUFFERRING")
            {
                criteria.condition_type_ = Bufferring;
                criteria.target_resource_type_ = Vod;
            }
            else if (upper_case_value == "BUFFERRING")
            {
                criteria.condition_type_ = Bufferring;
                criteria.target_resource_type_ = Any;
            }
        }
        
        const TiXmlElement* resources_element = criteria_element->FirstChildElement(Elements::Resources);
        if (resources_element)
        {
            std::vector<RID> resource_ids;
            for(const TiXmlElement* resource_element = resources_element->FirstChildElement(Elements::RID);
                resource_element != 0;
                resource_element = resource_element->NextSiblingElement(Elements::RID))
            {
                const string rid_value = resource_element->GetText();
                if (rid_value.length() > 0)
                {
                    resource_ids.push_back(RID(rid_value));
                }
            }

            criteria.resource_filter_.reset(new ResourceFilterByRid(resource_ids));
        }
        else
        {
            criteria.resource_filter_.reset(new ResourceFilterAny());
        }
    }

    const char* StatisticsConfigurationsParser::GetChildElementText(const tinyxml::TiXmlElement* parent_element, string child_element_name)
    {
        assert(parent_element);
        const TiXmlNode* child_element = parent_element->FirstChild(child_element_name);
        if (child_element && child_element->ToElement())
        {
            return child_element->ToElement()->GetText();
        }
        
        return 0;
    }

    void StatisticsConfigurationsParser::SetDefaultValues(StatisticsConfigurations& config)
    {
        if (config.expires_in_minutes_ < 0 || config.expires_in_minutes_ > 30*24*60)
        {
            config.expires_in_minutes_ = DefaultValues::ExpiresInMinutes;
        }
    }

    bool StatisticsConfigurationsParser::IsKnownStatisticsConfigurationChildElement(const string& element_name)
    {
        return element_name == Elements::CollectionCriteria ||
               element_name == Elements::CollectionLength ||
               element_name == Elements::MaxReportingOccurrence;
    }
}
