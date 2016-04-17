#include "CombatLogWnd.h"

#include <GG/Layout.h>
#include "../LinkText.h"

#include "../../client/human/HumanClientApp.h"
#include "../../combat/CombatLogManager.h"
#include "../../combat/CombatEvents.h"
#include "../../universe/System.h"
#include "../../util/AppInterface.h"
#include "../../util/i18n.h"
#include "../../util/Logger.h"
#include "../../universe/UniverseObject.h"
#include "../AccordionPanel.h"
#include "../../Empire/Empire.h"

namespace {
    const std::string EMPTY_STRING;

    std::map<int, int> CountByOwner(const std::set<int>& objects) {
        std::map<int, int> objects_per_owner;
        for (std::set<int>::const_iterator it = objects.begin(); it != objects.end(); ++it) {
            TemporaryPtr<const UniverseObject> object = Objects().Object(*it);
            if (object && (
                    object->ObjectType() == OBJ_SHIP || (
                        object->GetMeter(METER_POPULATION) &&
                        object->CurrentMeterValue(METER_POPULATION) > 0.0)))
            {
                int owner_id = object->Owner();
                if (objects_per_owner.find(owner_id) == objects_per_owner.end())
                    objects_per_owner[owner_id] = 0;
                ++objects_per_owner[owner_id];
            }
        }
        return objects_per_owner;
    }

    std::string CountsToText(const std::map<int, int>& count_per_empire, const std::string& delimiter = ", ") {
        std::stringstream ss;
        for (std::map<int,int>::const_iterator it = count_per_empire.begin(); it != count_per_empire.end(); ) {
            std::string owner_string = UserString("NEUTRAL");
            if (const Empire* owner = GetEmpire(it->first))
                owner_string = GG::RgbaTag(owner->Color()) + owner->Name() + "</rgba>";
            ss << owner_string << ": " << it->second;
            ++it;
            if (it != count_per_empire.end())
                ss << delimiter;
        }
        return ss.str();
    }

    const std::string& LinkTag(UniverseObjectType obj_type) {
        switch (obj_type) {
        case OBJ_SHIP:
            return VarText::SHIP_ID_TAG;
            break;
        case OBJ_FLEET:
            return VarText::FLEET_ID_TAG;
            break;
        case OBJ_PLANET:
            return VarText::PLANET_ID_TAG;
            break;
        case OBJ_BUILDING:
            return VarText::BUILDING_ID_TAG;
            break;
        case OBJ_SYSTEM:
            return VarText::SYSTEM_ID_TAG;
            break;
        case OBJ_FIELD:
        default:
            return EMPTY_STRING;
        }
    }

    /// Creates a link tag of the appropriate type for object_id,
    /// with the content being the public name from the point of view of client_empire_id.
    /// Returns not_found if object_id is not found.
    std::string PublicNameLink(int client_empire_id, int object_id, const std::string& not_found) {
        TemporaryPtr<const UniverseObject> object = GetUniverseObject(object_id);
        if (object) {
            const std::string& name = object->PublicName(client_empire_id);
            const std::string& tag = LinkTag(object->ObjectType());
            return LinkTaggedIDText(tag, object_id, name);
        } else {
            return not_found;
        }
    }

    std::string EmpireColourWrappedText(int empire_id, const std::string& text) {
        const Empire* empire = GetEmpire(empire_id);
        if (!empire)
            return text;
        return GG::RgbaTag(empire->Color()) + text + "</rgba>";
    }
}

CombatLogWnd::CombatLogWnd(GG::X w, GG::Y h) :
    GG::Wnd(GG::X0, GG::Y0, w, h, GG::NO_WND_FLAGS),
    m_text_format_flags(GG::FORMAT_WORDBREAK| GG::FORMAT_LEFT | GG::FORMAT_TOP)
{
    SetName("CombatLogWnd");
}

void CombatLogWnd::HandleLinkClick(const std::string& link_type, const std::string& data)
{ LinkClickedSignal(link_type, data); }
void CombatLogWnd::HandleLinkDoubleClick(const std::string& link_type, const std::string& data)
{ LinkDoubleClickedSignal(link_type, data); }
void CombatLogWnd::HandleLinkRightClick(const std::string& link_type, const std::string& data)
{ LinkRightClickedSignal(link_type, data); }


LinkText * CombatLogWnd::DecorateLinkText(std::string const & text) {
    LinkText * links = new LinkText(GG::X0, GG::Y0, text, ClientUI::GetFont(), GG::CLR_WHITE);

    links->SetTextFormat(m_text_format_flags);

    links->SetDecorator(VarText::SHIP_ID_TAG, new ColorByOwner());
    links->SetDecorator(VarText::PLANET_ID_TAG, new ColorByOwner());

    GG::Connect(links->LinkClickedSignal,       &CombatLogWnd::HandleLinkClick,          this);
    GG::Connect(links->LinkDoubleClickedSignal, &CombatLogWnd::HandleLinkDoubleClick,    this);
    GG::Connect(links->LinkRightClickedSignal,  &CombatLogWnd::HandleLinkDoubleClick,    this);

    return links;
}

void CombatLogWnd::AddRow(GG::Wnd * wnd) {
    if( GG::Layout * layout = GetLayout())
        layout->Add(wnd, layout->Rows(), 0);
}

void CombatLogWnd::SetLog(int log_id) {
    if (!CombatLogAvailable(log_id)) {
        ErrorLogger() << "Couldn't find combat log with id: " << log_id;
        return;
    }

    DeleteChildren();
    GG::Layout* layout = new GG::Layout(UpperLeft().x, UpperLeft().y, Width(), Height()
                                        , 1, 1 ///< numrows, numcols
                                        , 0, 0 ///< wnd margin, cell margin
                                       );
    SetLayout(layout);

    const CombatLog& log = GetCombatLog(log_id);
    int client_empire_id = HumanClientApp::GetApp()->EmpireID();

    DebugLogger() << "Setting log with " << log.combat_events.size() << " events";

    TemporaryPtr<const System> system = GetSystem(log.system_id);
    const std::string& sys_name = (system ? system->PublicName(client_empire_id) : UserString("ERROR"));

    AddRow(DecorateLinkText(str(FlexibleFormat(UserString("ENC_COMBAT_LOG_DESCRIPTION_STR"))
                                % LinkTaggedIDText(VarText::SYSTEM_ID_TAG, log.system_id, sys_name)
                                % log.turn) + "\n"
                           ));
    AddRow(DecorateLinkText(UserString("COMBAT_INITIAL_FORCES")));
    AddRow(DecorateLinkText(CountsToText(CountByOwner(log.object_ids))));

    for (std::vector<CombatEventPtr>::const_iterator it = log.combat_events.begin();
         it != log.combat_events.end(); ++it) {
        DebugLogger() << "event debug info: " << it->get()->DebugString();

        if (const BoutBeginEvent* bout_begin = dynamic_cast<BoutBeginEvent*>(it->get())) {
            AddRow(DecorateLinkText(str(FlexibleFormat(UserString("ENC_ROUND_BEGIN")) % bout_begin->bout)));

        } else if (const AttackEvent* attack = dynamic_cast<AttackEvent*>(it->get())) {
            std::stringstream event_title;
            std::stringstream event_detail;
            std::string attacker_link;
            if (attack->attacker_id >= 0)   // ship
                attacker_link = PublicNameLink(client_empire_id, attack->attacker_id, UserString("ENC_COMBAT_UNKNOWN_OBJECT"));
            else                            // fighter
                attacker_link = EmpireColourWrappedText(attack->attacker_owner_id, UserString("OBJ_FIGHTER"));

            std::string target_link = PublicNameLink(client_empire_id, attack->target_id, UserString("ENC_COMBAT_UNKNOWN_OBJECT"));

            const std::string& template_str = UserString("ENC_COMBAT_ATTACK_STR");

            AddRow(DecorateLinkText(str(FlexibleFormat(template_str)
                                        % attacker_link
                                        % target_link
                                        % attack->damage
                                        % attack->bout
                                        % attack->round)));

            DebugLogger() << " Attack Event is " << event_title.str();


        } else if (const IncapacitationEvent* incapacitation = dynamic_cast<IncapacitationEvent*>(it->get())) {
            TemporaryPtr<const UniverseObject> object = GetUniverseObject(incapacitation->object_id);
            std::string template_str, object_str;
            int owner_id = incapacitation->object_owner_id;

            if (!object && incapacitation->object_id < 0) {
                template_str = UserString("ENC_COMBAT_FIGHTER_INCAPACITATED_STR");
                object_str = UserString("OBJ_FIGHTER");

            } else if (!object) {
                template_str = UserString("ENC_COMBAT_UNKNOWN_DESTROYED_STR");
                object_str = UserString("ENC_COMBAT_UNKNOWN_OBJECT");

            } else if (object->ObjectType() == OBJ_PLANET) {
                template_str = UserString("ENC_COMBAT_PLANET_INCAPACITATED_STR");
                object_str = PublicNameLink(client_empire_id, incapacitation->object_id, UserString("ENC_COMBAT_UNKNOWN_OBJECT"));

            } else {    // ships or other to-be-determined objects...
                template_str = UserString("ENC_COMBAT_DESTROYED_STR");
                object_str = PublicNameLink(client_empire_id, incapacitation->object_id, UserString("ENC_COMBAT_UNKNOWN_OBJECT"));
            }

            std::string owner_string = " ";
            if (const Empire* owner = GetEmpire(owner_id))
                owner_string += owner->Name() + " ";

            AddRow(DecorateLinkText(str(FlexibleFormat(template_str) % owner_string % object_str)));

        } else if (const FighterLaunchEvent* launch = dynamic_cast<FighterLaunchEvent*>(it->get())) {
            std::string launched_from_link = PublicNameLink(client_empire_id, launch->launched_from_id, UserString("ENC_COMBAT_UNKNOWN_OBJECT"));
            std::string empire_coloured_fighter = EmpireColourWrappedText(launch->fighter_owner_empire_id, UserString("OBJ_FIGHTER"));

            // launching negative fighters indicates recovery of them by the ship
            const std::string& template_str = (launch->number_launched >= 0 ?
                                                UserString("ENC_COMBAT_LAUNCH_STR") :
                                                UserString("ENC_COMBAT_RECOVER_STR"));

            AddRow(DecorateLinkText(str(FlexibleFormat(template_str)
                                        % launched_from_link
                                        % empire_coloured_fighter
                                        % std::abs(launch->number_launched)
                                        % attack->bout
                                        % attack->round)));
        } else if (const FighterAttackedEvent* fighter_attack = dynamic_cast<FighterAttackedEvent*>(it->get())) {
            std::string attacked_by;
            if (fighter_attack->attacked_by_object_id >= 0) // attacked by ship or planet
                attacked_by = PublicNameLink(client_empire_id, fighter_attack->attacked_by_object_id, UserString("ENC_COMBAT_UNKNOWN_OBJECT"));
            else                                            // attacked by fighter
                attacked_by = EmpireColourWrappedText(fighter_attack->attacker_owner_empire_id, UserString("OBJ_FIGHTER"));
            std::string empire_coloured_attacked_fighter = EmpireColourWrappedText(fighter_attack->attacked_owner_id, UserString("OBJ_FIGHTER"));

            const std::string& template_str = UserString("ENC_COMBAT_ATTACK_SIMPLE_STR");

            AddRow(DecorateLinkText(str(FlexibleFormat(template_str)
                                        % attacked_by
                                        % empire_coloured_attacked_fighter
                                        % attack->bout
                                        % attack->round)));
        }
    }

    std::stringstream summary_text;
    summary_text << "\n" + UserString("COMBAT_SUMMARY_DESTROYED")
        + "\n" + CountsToText(CountByOwner(log.destroyed_object_ids));

    AddRow(DecorateLinkText(summary_text.str()));

    //Add a dummy row that the layout manager can use to add space.
    AddRow(DecorateLinkText(""));
    layout->SetRowStretch(layout->Rows() - 1, 1);

}

GG::Pt CombatLogWnd::ClientUpperLeft() const
{ return UpperLeft() + GG::Pt(GG::X(MARGIN), GG::Y(MARGIN)); }

GG::Pt CombatLogWnd::ClientLowerRight() const
{ return LowerRight() - GG::Pt(GG::X(MARGIN), GG::Y(MARGIN)); }

void CombatLogWnd::Render() {
    GG::Clr interior_color =  ClientUI::CtrlColor();
    GG::Clr border_color = ClientUI::CtrlBorderColor();

    GG::Pt ul = UpperLeft(), lr = LowerRight();

    FlatRectangle(ul, lr, interior_color, border_color, 1);
}
