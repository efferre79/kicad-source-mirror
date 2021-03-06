/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

/**
 * @file  pcbnew_action_plugins.cpp
 * @brief Class PCBNEW_PYTHON_ACTION_PLUGINS
 */

#include "pcbnew_action_plugins.h"
#include <python_scripting.h>
#include <stdio.h>
#include <macros.h>
#include <pcbnew_id.h>
#include <menus_helpers.h>
#include <class_drawpanel.h>    // m_canvas
#include <class_board.h>
#include <class_module.h>
#include <class_track.h>
#include <class_drawsegment.h>
#include <class_zone.h>
#include <board_commit.h>
#include <kicad_device_context.h>

PYTHON_ACTION_PLUGIN::PYTHON_ACTION_PLUGIN( PyObject* aAction )
{
    PyLOCK lock;

    this->m_PyAction = aAction;
    Py_XINCREF( aAction );
}


PYTHON_ACTION_PLUGIN::~PYTHON_ACTION_PLUGIN()
{
    PyLOCK lock;

    Py_XDECREF( this->m_PyAction );
}


PyObject* PYTHON_ACTION_PLUGIN::CallMethod( const char* aMethod, PyObject* aArglist )
{
    PyLOCK lock;

    PyErr_Clear();
    // pFunc is a new reference to the desired method
    PyObject* pFunc = PyObject_GetAttrString( this->m_PyAction, aMethod );

    if( pFunc && PyCallable_Check( pFunc ) )
    {
        PyObject* result = PyObject_CallObject( pFunc, aArglist );

        if( PyErr_Occurred() )
        {
            wxMessageBox( PyErrStringWithTraceback(),
                    _( "Exception on python action plugin code" ),
                    wxICON_ERROR | wxOK );
        }

        if( result )
        {
            Py_XDECREF( pFunc );
            return result;
        }
    }
    else
    {
        wxString msg = wxString::Format( _( "Method \"%s\" not found, or not callable" ), aMethod );
        wxMessageBox( msg, _( "Unknown Method" ), wxICON_ERROR | wxOK );
    }

    if( pFunc )
    {
        Py_XDECREF( pFunc );
    }

    return NULL;
}


wxString PYTHON_ACTION_PLUGIN::CallRetStrMethod( const char* aMethod, PyObject* aArglist )
{
    wxString    ret;
    PyLOCK      lock;

    PyObject* result = CallMethod( aMethod, aArglist );

    if( result )
    {
        const char* str_res = PyString_AsString( result );
        ret = FROM_UTF8( str_res );
        Py_DECREF( result );
    }

    return ret;
}


wxString PYTHON_ACTION_PLUGIN::GetCategoryName()
{
    PyLOCK lock;

    return CallRetStrMethod( "GetCategoryName" );
}


wxString PYTHON_ACTION_PLUGIN::GetName()
{
    PyLOCK lock;

    return CallRetStrMethod( "GetName" );
}


wxString PYTHON_ACTION_PLUGIN::GetDescription()
{
    PyLOCK lock;

    return CallRetStrMethod( "GetDescription" );
}


bool PYTHON_ACTION_PLUGIN::GetShowToolbarButton()
{
    PyLOCK lock;

    PyObject* result = CallMethod( "GetShowToolbarButton");

    return PyObject_IsTrue(result);
}


wxString PYTHON_ACTION_PLUGIN::GetIconFileName()
{
    PyLOCK lock;

    return CallRetStrMethod( "GetIconFileName" );
}


wxString PYTHON_ACTION_PLUGIN::GetPluginPath()
{
    PyLOCK lock;

    return CallRetStrMethod( "GetPluginPath" );
}


void PYTHON_ACTION_PLUGIN::Run()
{
    PyLOCK lock;

    CallMethod( "Run" );
}


void* PYTHON_ACTION_PLUGIN::GetObject()
{
    return (void*) m_PyAction;
}


void PYTHON_ACTION_PLUGINS::register_action( PyObject* aPyAction )
{
    PYTHON_ACTION_PLUGIN* fw = new PYTHON_ACTION_PLUGIN( aPyAction );

    fw->register_action();
}


void PYTHON_ACTION_PLUGINS::deregister_action( PyObject* aPyAction )
{
    // deregister also destroys the previously created "PYTHON_ACTION_PLUGIN object"
    ACTION_PLUGINS::deregister_object( (void*) aPyAction );
}


#if defined(KICAD_SCRIPTING) && defined(KICAD_SCRIPTING_ACTION_MENU)

void PCB_EDIT_FRAME::OnActionPluginMenu( wxCommandEvent& aEvent )
{
    ACTION_PLUGIN* actionPlugin = ACTION_PLUGINS::GetActionByMenu( aEvent.GetId() );

	if( actionPlugin )
        RunActionPlugin( actionPlugin );
}

void PCB_EDIT_FRAME::OnActionPluginButton( wxCommandEvent& aEvent )
{
	ACTION_PLUGIN* actionPlugin = ACTION_PLUGINS::GetActionByButton( aEvent.GetId() );

	if( actionPlugin )
        RunActionPlugin( actionPlugin );
}

void PCB_EDIT_FRAME::RunActionPlugin( ACTION_PLUGIN* aActionPlugin )
{
    PICKED_ITEMS_LIST itemsList;
    BOARD*  currentPcb  = GetBoard();
    bool    fromEmpty   = false;

    itemsList.m_Status = UR_CHANGED;

    OnModify();

    // Append tracks:
    for( BOARD_ITEM* item = currentPcb->m_Track; item != NULL; item = item->Next() )
    {
        ITEM_PICKER picker( item, UR_CHANGED );
        itemsList.PushItem( picker );
    }

    // Append modules:
    for( BOARD_ITEM* item = currentPcb->m_Modules; item != NULL; item = item->Next() )
    {
        ITEM_PICKER picker( item, UR_CHANGED );
        itemsList.PushItem( picker );
    }

    // Append drawings
    for( BOARD_ITEM* item = currentPcb->m_Drawings; item != NULL; item = item->Next() )
    {
        ITEM_PICKER picker( item, UR_CHANGED );
        itemsList.PushItem( picker );
    }

    // Append zones outlines
    for( int ii = 0; ii < currentPcb->GetAreaCount(); ii++ )
    {
        ITEM_PICKER picker( (EDA_ITEM*) currentPcb->GetArea(
                        ii ), UR_CHANGED );
        itemsList.PushItem( picker );
    }

    // Append zones segm:
    for( BOARD_ITEM* item = currentPcb->m_SegZoneDeprecated; item != NULL; item = item->Next() )
    {
        ITEM_PICKER picker( item, UR_CHANGED );
        itemsList.PushItem( picker );
    }

    if( itemsList.GetCount() > 0 )
        SaveCopyInUndoList( itemsList, UR_CHANGED, wxPoint( 0.0, 0.0 ) );
    else
        fromEmpty = true;

    itemsList.ClearItemsList();

    // Execute plugin itself...
    ACTION_PLUGINS::SetActionRunning( true );
    aActionPlugin->Run();
    ACTION_PLUGINS::SetActionRunning( false );

    currentPcb->m_Status_Pcb = 0;

    // Get back the undo buffer to fix some modifications
    PICKED_ITEMS_LIST* oldBuffer = NULL;

    if( fromEmpty )
    {
        oldBuffer = new PICKED_ITEMS_LIST();
        oldBuffer->m_Status = UR_NEW;
    }
    else
    {
        oldBuffer = GetScreen()->PopCommandFromUndoList();
        wxASSERT( oldBuffer );
    }

    // Try do discover what was modified

    PICKED_ITEMS_LIST deletedItemsList;

    // Found deleted modules
    for( unsigned int i = 0; i < oldBuffer->GetCount(); i++ )
    {
        BOARD_ITEM* item = (BOARD_ITEM*) oldBuffer->GetPickedItem( i );
        ITEM_PICKER picker( item, UR_DELETED );

        wxASSERT( item );

        switch( item->Type() )
        {
        case PCB_NETINFO_T:
        case PCB_MARKER_T:
        case PCB_MODULE_T:
        case PCB_TRACE_T:
        case PCB_VIA_T:
        case PCB_LINE_T:
        case PCB_TEXT_T:
        case PCB_DIMENSION_T:
        case PCB_TARGET_T:
        case PCB_ZONE_T:

            // If item has a list it's mean that the element is on the board
            if( item->GetList() == NULL )
            {
                deletedItemsList.PushItem( picker );
            }

            break;

        case PCB_ZONE_AREA_T:
        {
            bool zoneFound = false;

            for( int ii = 0; ii < currentPcb->GetAreaCount(); ii++ )
                zoneFound |= currentPcb->GetArea( ii ) == item;

            if( !zoneFound )
            {
                deletedItemsList.PushItem( picker );
            }

            break;
        }

        default:
            wxString msg;
            msg.Printf( _( "(PCB_EDIT_FRAME::OnActionPlugin) needs work: "
                                "BOARD_ITEM type (%d) not handled" ),
                    item->Type() );
            wxFAIL_MSG( msg );
            break;
        }
    }

    // Mark deleted elements in undolist
    for( unsigned int i = 0; i < deletedItemsList.GetCount(); i++ )
    {
        oldBuffer->PushItem( deletedItemsList.GetItemWrapper( i ) );
    }

    // Find new modules
    for( BOARD_ITEM* item = currentPcb->m_Modules; item != NULL; item = item->Next() )
    {
        if( !oldBuffer->ContainsItem( item ) )
        {
            ITEM_PICKER picker( item, UR_NEW );
            oldBuffer->PushItem( picker );
        }
    }

    for( BOARD_ITEM* item = currentPcb->m_Track; item != NULL; item = item->Next() )
    {
        if( !oldBuffer->ContainsItem( item ) )
        {
            ITEM_PICKER picker( item, UR_NEW );
            oldBuffer->PushItem( picker );
        }
    }

    for( BOARD_ITEM* item = currentPcb->m_Drawings; item != NULL; item = item->Next() )
    {
        if( !oldBuffer->ContainsItem( item ) )
        {
            ITEM_PICKER picker( item, UR_NEW );
            oldBuffer->PushItem( picker );
        }
    }

    for( BOARD_ITEM* item = currentPcb->m_SegZoneDeprecated; item != NULL; item = item->Next() )
    {
        if( !oldBuffer->ContainsItem( item ) )
        {
            ITEM_PICKER picker( item, UR_NEW );
            oldBuffer->PushItem( picker );
        }
    }

    for( int ii = 0; ii < currentPcb->GetAreaCount(); ii++ )
    {
        if( !oldBuffer->ContainsItem( (EDA_ITEM*) currentPcb->GetArea( ii ) ) )
        {
            ITEM_PICKER picker( (EDA_ITEM*) currentPcb->GetArea(
                            ii ), UR_NEW );
            oldBuffer->PushItem( picker );
        }
    }


    GetScreen()->PushCommandToUndoList( oldBuffer );

    if( IsGalCanvasActive() )
    {
        UseGalCanvas( GetGalCanvas() );
    }
    else
    {
        UpdateUserInterface();
        GetScreen()->SetModify();
        Refresh();
    }
}


void PCB_EDIT_FRAME::RebuildActionPluginMenus()
{
    wxMenu* actionMenu = GetMenuBar()->FindItem( ID_TOOLBARH_PCB_ACTION_PLUGIN )->GetSubMenu();

    if( !actionMenu ) // Should not occur.
        return;

    // First, remove existing submenus, if they are too many
    wxMenuItemList list = actionMenu->GetMenuItems();
    // The first menuitems are the refresh menu and separator. do not count them
    int act_menu_count = -2;

    std::vector<wxMenuItem*> available_menus;

    for( auto iter = list.begin(); iter != list.end(); ++iter, act_menu_count++ )
    {
        if( act_menu_count < 0 )
            continue;

        wxMenuItem* item = *iter;

        if( act_menu_count < ACTION_PLUGINS::GetActionsCount() )
        {
            available_menus.push_back( item );
            continue;
        }

        // Remove menus which are not usable for our current plugin list
        Disconnect( item->GetId(), wxEVT_COMMAND_MENU_SELECTED,
                (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) &
                PCB_EDIT_FRAME::OnActionPluginMenu );
        actionMenu->Delete( item );
    }

    for( int ii = 0; ii < ACTION_PLUGINS::GetActionsCount(); ii++ )
    {
        wxMenuItem* item;
        ACTION_PLUGIN* ap = ACTION_PLUGINS::GetAction( ii );
        const wxBitmap& bitmap = ap->iconBitmap.IsOk() ? ap->iconBitmap : KiBitmap( hammer_xpm );

        if( ii < (int) available_menus.size() )
        {
            item = available_menus[ii];
            item->SetItemLabel( ap->GetName() );
            item->SetHelp( ap->GetDescription() );

            // On windows we need to set "unchecked" bitmap
#if defined(__WXMSW__)
            item->SetBitmap( bitmap, false );
#else
            item->SetBitmap( bitmap );
#endif
        }
        else
        {
            item = AddMenuItem( actionMenu, wxID_ANY,
                    ap->GetName(),
                    ap->GetDescription(),
                    bitmap );

            Connect( item->GetId(), wxEVT_COMMAND_MENU_SELECTED,
                    (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) &
                    PCB_EDIT_FRAME::OnActionPluginMenu );
        }

        ACTION_PLUGINS::SetActionMenu( ii, item->GetId() );
    }
}


void PCB_EDIT_FRAME::AddActionPluginTools()
{
    bool need_separator = true;
    const auto& orderedPlugins = GetOrderedActionPlugins();

    for( const auto& ap : orderedPlugins )
    {
        if( GetActionPluginButtonVisible( ap->GetPluginPath(), ap->GetShowToolbarButton() ) )
        {

            if ( need_separator )
            {
                KiScaledSeparator( m_mainToolBar, this );
                need_separator = false;
            }

            // Add button
            wxBitmap bitmap;

            if ( ap->iconBitmap.IsOk() )
                bitmap = KiScaledBitmap( ap->iconBitmap, this );
            else
                bitmap = KiScaledBitmap( hammer_xpm, this );

            wxAuiToolBarItem* button = m_mainToolBar->AddTool(
                    wxID_ANY, wxEmptyString, bitmap, ap->GetName() );

            Connect( button->GetId(), wxEVT_COMMAND_MENU_SELECTED,
                    (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) &
                    PCB_EDIT_FRAME::OnActionPluginButton );

            // Link action plugin to button
            ACTION_PLUGINS::SetActionButton( ap, button->GetId() );
        }
    }
}


void PCB_EDIT_FRAME::SetActionPluginSettings( const std::vector< std::pair<wxString, wxString> >& aPluginSettings )
{
    m_configSettings.m_pluginSettings = aPluginSettings;
    ReCreateHToolbar();
}


std::vector< std::pair<wxString, wxString> > PCB_EDIT_FRAME::GetActionPluginSettings()
{
    return m_configSettings.m_pluginSettings;
}


std::vector<ACTION_PLUGIN*> PCB_EDIT_FRAME::GetOrderedActionPlugins()
{
    std::vector<ACTION_PLUGIN*> orderedPlugins;
    const auto& pluginSettings = GetActionPluginSettings();

    // First add plugins that have entries in settings
    for( size_t ii = 0; ii < pluginSettings.size(); ii++ )
    {
        for( int jj = 0; jj < ACTION_PLUGINS::GetActionsCount(); jj++ )
        {
            if( ACTION_PLUGINS::GetAction( jj )->GetPluginPath() == pluginSettings[ii].first )
                orderedPlugins.push_back( ACTION_PLUGINS::GetAction( jj ) );
        }
    }

    // Now append new plugins that have not been configured yet
    for( int ii = 0; ii < ACTION_PLUGINS::GetActionsCount(); ii++ )
    {
        bool found = false;

        for( size_t jj = 0; jj < orderedPlugins.size(); jj++ )
        {
            if( ACTION_PLUGINS::GetAction( ii ) == orderedPlugins[jj] )
                found = true;
        }

        if ( !found )
            orderedPlugins.push_back( ACTION_PLUGINS::GetAction( ii ) );
    }

    return orderedPlugins;
}


bool PCB_EDIT_FRAME::GetActionPluginButtonVisible( const wxString& aPluginPath, bool aPluginDefault )
{
    auto& settings = m_configSettings.m_pluginSettings;

    for(const auto& entry : settings )
    {
        if (entry.first == aPluginPath )
            return entry.second == wxT( "Visible" );
    }

    // Plugin is not in settings, return default.
    return  aPluginDefault;
}


#endif
