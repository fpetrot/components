/*
 *  This file is part of Rabbits
 *  Copyright (C) 2015  Clement Deschamps and Luc Michel
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "node.h"

#define DEBUG_TRACE
#include "rabbits/logger.h"


#define ROUTING_3D z_first_routing //xy_first_routing
#define ROUTING_2D x_first_routing //y_first_routing


#define CONC_(A,B) A ## B
#define CONC(A,B) CONC_ (A,B)
#define ROUTING_2D_DBG CONC(ROUTING_2D,_dbg)
#define ROUTING_3D_DBG CONC(ROUTING_3D,_dbg)

template <unsigned int BUSWIDTH >
Node<BUSWIDTH>::Node(sc_core::sc_module_name name,InterconnectNoc<BUSWIDTH> *p,
                        uint16_t a,uint16_t b,uint16_t c, int t) : sc_core::sc_module(name)
{
    int i;
    x = a;
    y = b;
    z = c;
    type = t;
    p_inter = p;
    for (i=0;i<4;i++) {
        m_targets[i]=new tlm::tlm_target_socket<BUSWIDTH>;
        m_initiators[i]=new tlm::tlm_initiator_socket<BUSWIDTH>;
        m_targets[i]->bind(*this);
        m_initiators[i]->bind(*this);
    }

    target_slave = NULL;
    initiator_master = NULL;
    connect = -1;
}

template <unsigned int BUSWIDTH >
void Node<BUSWIDTH>::init_up(void)
{
    m_target_up=new tlm::tlm_target_socket<BUSWIDTH>;
    m_initiator_up=new tlm::tlm_initiator_socket<BUSWIDTH>;
    m_target_up->bind(*this);
    m_initiator_up->bind(*this);

}

template <unsigned int BUSWIDTH >
void Node<BUSWIDTH>::init_down(void)
{
    m_target_down=new tlm::tlm_target_socket<BUSWIDTH>;
    m_initiator_down=new tlm::tlm_initiator_socket<BUSWIDTH>;
    m_target_down->bind(*this);
    m_initiator_down->bind(*this);
}

template <unsigned int BUSWIDTH >
Node<BUSWIDTH>::~Node()
{
    int i;
    for (i=0;i<4;i++) {
        delete m_targets[i];
        delete m_initiators[i];
    }
    if (target_slave) {
        delete target_slave;
    }
    if (initiator_master) {
       delete initiator_master;
    }
}

template <unsigned int BUSWIDTH >
void Node<BUSWIDTH>::b_transport(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
{
    RouteInfo *route;
    wait(3, SC_NS);

    trans.get_extension(route);

    if (!route) {
        ERR_PRINTF("Received message error\n");
        exit(1);
    }
    DBG_PRINTF("N[%d,%d,%d] Routing message to (%d,%d,%d) with address = %08llx at t=%lu \n",
            this->x, this->y, this->z, route->x, route->y, route->z,
            trans.get_address(), sc_core::sc_time_stamp().value()/1000);

    if (!ROUTING_3D(trans,delay,route))
        ERR_PRINTF("ERROR Routing Transaction\n");

}

template <unsigned int BUSWIDTH >
void Node<BUSWIDTH>::b_transport_master(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time& delay)
{
    sc_dt::uint64 target_address;
    int i;
    RouteInfo *route=new RouteInfo;

    wait(3, SC_NS);

    i = p_inter->decode_address_i(trans.get_address(), target_address);
    if (i == -1) {
        ERR_PRINTF("Cannot find slave at address %08llx\n", trans.get_address());
        exit(1);
    }


    trans.set_address(trans.get_address()-target_address);
    trans.set_extension(route);
    route->x=p_inter->get_route_x(i);
    route->y=p_inter->get_route_y(i);
    route->z=p_inter->get_route_z(i);

    DBG_PRINTF("N[%d,%d,%d] Routing message from master to (%d,%d,%d) with address = %08llx at t=%lu\n",
            this->x, this->y, this->z, route->x, route->y, route->z,
            trans.get_address(), sc_core::sc_time_stamp().value()/1000);

    if (!ROUTING_3D(trans,delay,route))
        ERR_PRINTF("ERROR Routing Transaction\n");

}

template <unsigned int BUSWIDTH >
void Node<BUSWIDTH>::register_transport_master() {
    this->initiator_master->register_b_transport(this,
                                               &Node::b_transport_master);
    this->initiator_master->register_transport_dbg(this,
                                               &Node::transport_dbg_master);
    this->initiator_master->register_get_direct_mem_ptr(this,
                                               &Node::get_direct_mem_ptr);
}

template <unsigned int BUSWIDTH >
unsigned int Node<BUSWIDTH>::transport_dbg(tlm::tlm_generic_payload& trans)
{
    RouteInfo *route;

    trans.get_extension(route);

    if (!route) {
        ERR_PRINTF("Received message error\n");
        exit(1);
    }

    return ROUTING_3D_DBG(trans,route);
}

template <unsigned int BUSWIDTH >
unsigned int Node<BUSWIDTH>::transport_dbg_master(tlm::tlm_generic_payload& trans) {

    sc_dt::uint64 target_address;
    int i;
    RouteInfo *route=new RouteInfo;

    i = p_inter->decode_address_i(trans.get_address(), target_address);
    if (i == -1) {
        ERR_PRINTF("Cannot find slave at address %08llx\n", trans.get_address());
        exit(1);
    }

    trans.set_address(trans.get_address()-target_address);
    trans.set_extension(route);
    route->x=p_inter->get_route_x(i);
    route->y=p_inter->get_route_y(i);
    route->z=p_inter->get_route_z(i);


    return ROUTING_3D_DBG(trans,route);
}

template <unsigned int BUSWIDTH>
bool Node<BUSWIDTH>::get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                        tlm::tlm_dmi& dmi_data)
{
    bool ret;
    sc_dt::uint64 offset;
    int i;
    tlm::tlm_initiator_socket<BUSWIDTH> *target;

    i = p_inter->decode_address_i(trans.get_address(), offset);
    if (i == -1) {
        return false;
    }

    target=p_inter->get_slave_target(i);
    trans.set_address(trans.get_address() - offset);

    ret = (*target)->get_direct_mem_ptr(trans, dmi_data);

    if (ret) {
        dmi_data.set_start_address(dmi_data.get_start_address() + offset);
        dmi_data.set_end_address(dmi_data.get_end_address() + offset);
    }

    return ret;
}

/* Start Routing Algo */

template <unsigned int BUSWIDTH >
int Node<BUSWIDTH>::z_first_routing(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time &delay, RouteInfo *route)
{


    if (route->z > this->z)
        (*m_initiator_up)->b_transport(trans, delay);
    else if (route->z < this->z)
        (*m_initiator_down)->b_transport(trans, delay);
    else if(ROUTING_2D(trans,delay,route))
        return 1;
    else if (target_slave)
        (*target_slave)->b_transport(trans,delay);
    else
        return 0;

    return 1;
}

template <unsigned int BUSWIDTH >
int Node<BUSWIDTH>::xy_first_routing(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time &delay, RouteInfo *route)
{


    if (ROUTING_2D(trans,delay,route))
        return 1;
    else if (route->z > this->z)
        (*m_initiator_up)->b_transport(trans, delay);
    else if (route->z < this->z)
        (*m_initiator_down)->b_transport(trans, delay);
    else if (target_slave)
        (*target_slave)->b_transport(trans,delay);
    else
        return 0;

    return 1;
}

template <unsigned int BUSWIDTH >
int Node<BUSWIDTH>::x_first_routing(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time &delay, RouteInfo *route)
{
    uint16_t d_x,d_y;
    int diff_x = route->x - this->x;
    int diff_y = route->y - this->y;

    if (this->type == TORUS) {
        d_x = p_inter->get_X()/2;
        d_y = p_inter->get_Y()/2;
        if ( (diff_x > 0 && diff_x <= d_x) || (diff_x < 0 && diff_x < -d_x) )
            (*m_initiators[2])->b_transport(trans, delay);
        else if ( (diff_x > 0 && diff_x > d_x) || (diff_x < 0 && diff_x >= -d_x) )
            (*m_initiators[0])->b_transport(trans, delay);
        else if ( (diff_y > 0 && diff_y <= d_y) || (diff_y < 0 && diff_y < -d_y) )
            (*m_initiators[1])->b_transport(trans, delay);
        else if ( (diff_y > 0 && diff_y > d_y) || (diff_y < 0 && diff_y >= -d_y) )
            (*m_initiators[3])->b_transport(trans, delay);
        else
            return 0;
    } else {
        if (diff_x > 0)
            (*m_initiators[2])->b_transport(trans, delay);
        else if (diff_x < 0)
            (*m_initiators[0])->b_transport(trans, delay);
        else if (diff_y > 0)
            (*m_initiators[1])->b_transport(trans, delay);
        else if (diff_y < 0)
            (*m_initiators[3])->b_transport(trans, delay);
        else
            return 0;
    }
    return 1;
}

template <unsigned int BUSWIDTH >
int Node<BUSWIDTH>::y_first_routing(tlm::tlm_generic_payload& trans,
                                        sc_core::sc_time &delay, RouteInfo *route)
{
    uint16_t d_x,d_y;
    int diff_x = route->x - this->x;
    int diff_y = route->y - this->y;

    if (this->type == TORUS) {
        d_x = p_inter->get_X()/2;
        d_y = p_inter->get_Y()/2;
        if ( (diff_y > 0 && diff_y <= d_y) || (diff_y < 0 && diff_y < -d_y) )
            (*m_initiators[1])->b_transport(trans, delay);
        else if ( (diff_y > 0 && diff_y > d_y) || (diff_y < 0 && diff_y >= -d_y) )
            (*m_initiators[3])->b_transport(trans, delay);
	else if ( (diff_x > 0 && diff_x <= d_x) || (diff_x < 0 && diff_x < -d_x) )
            (*m_initiators[2])->b_transport(trans, delay);
        else if ( (diff_x > 0 && diff_x > d_x) || (diff_x < 0 && diff_x >= -d_x) )
            (*m_initiators[0])->b_transport(trans, delay);
        else
            return 0;
    } else {
        if (diff_y > 0)
            (*m_initiators[1])->b_transport(trans, delay);
        else if (diff_y < 0)
            (*m_initiators[3])->b_transport(trans, delay);
        else if (diff_x > 0)
            (*m_initiators[2])->b_transport(trans, delay);
        else if (diff_x < 0)
            (*m_initiators[0])->b_transport(trans, delay);
        else
            return 0;
    }
    return 1;
}

template <unsigned int BUSWIDTH >
unsigned int Node<BUSWIDTH>::z_first_routing_dbg(tlm::tlm_generic_payload& trans,
                                        RouteInfo *route)
{


    unsigned int tmp;
    tmp = ROUTING_2D_DBG(trans,route);
    if (route->z > this->z)
        return (*m_initiator_up)->transport_dbg(trans);
    else if (route->z < this->z)
        return (*m_initiator_down)->transport_dbg(trans);
    else if(tmp)
        return tmp;
    else if (target_slave)
        return (*target_slave)->transport_dbg(trans);
    else
        return 0;

}

template <unsigned int BUSWIDTH >
unsigned int Node<BUSWIDTH>::xy_first_routing_dbg(tlm::tlm_generic_payload& trans,
                                        RouteInfo *route)
{

    unsigned int tmp;
    tmp = ROUTING_2D_DBG(trans,route);
    if (tmp)
        return tmp;
    else if (route->z > this->z)
        return (*m_initiator_up)->transport_dbg(trans);
    else if (route->z < this->z)
        return (*m_initiator_down)->transport_dbg(trans);
    else if (target_slave)
        return (*target_slave)->transport_dbg(trans);
    else
        return 0;
}

template <unsigned int BUSWIDTH >
unsigned int Node<BUSWIDTH>::x_first_routing_dbg(tlm::tlm_generic_payload& trans,
                                        RouteInfo *route)
{
    uint16_t d_x,d_y;
    int diff_x = route->x - this->x;
    int diff_y = route->y - this->y;

    if (this->type == TORUS) {
        d_x = p_inter->get_X()/2;
        d_y = p_inter->get_Y()/2;
        if ( (diff_x > 0 && diff_x < d_x) || (diff_x < 0 && diff_x <= -d_x) )
            return (*m_initiators[2])->transport_dbg(trans);
        else if ( (diff_x > 0 && diff_x >= d_x) || (diff_x < 0 && diff_x > -d_x) )
            return (*m_initiators[0])->transport_dbg(trans);
        else if ( (diff_y > 0 && diff_y < d_y) || (diff_y < 0 && diff_y <= -d_y) )
            return (*m_initiators[1])->transport_dbg(trans);
        else if ( (diff_y > 0 && diff_y >= d_y) || (diff_y < 0 && diff_y > -d_y) )
            return (*m_initiators[3])->transport_dbg(trans);
        else
            return 0;
    } else {
        if (diff_x > 0)
            return (*m_initiators[2])->transport_dbg(trans);
        else if (diff_x < 0)
            return (*m_initiators[0])->transport_dbg(trans);
        else if (diff_y > 0)
            return (*m_initiators[1])->transport_dbg(trans);
        else if (diff_y < 0)
            return (*m_initiators[3])->transport_dbg(trans);
        else
            return 0;
    }
    return 1;
}

template <unsigned int BUSWIDTH >
unsigned int Node<BUSWIDTH>::y_first_routing_dbg(tlm::tlm_generic_payload& trans,
                                        RouteInfo *route)
{
    uint16_t d_x,d_y;
    int diff_x = route->x - this->x;
    int diff_y = route->y - this->y;

    if (this->type == TORUS) {
        d_x = p_inter->get_X()/2;
        d_y = p_inter->get_Y()/2;
        if ( (diff_y > 0 && diff_y < d_y) || (diff_y < 0 && diff_y <= -d_y) )
            return (*m_initiators[1])->transport_dbg(trans);
        else if ( (diff_y > 0 && diff_y >= d_y) || (diff_y < 0 && diff_y > -d_y) )
            return (*m_initiators[3])->transport_dbg(trans);
        else if ( (diff_x > 0 && diff_x < d_x) || (diff_x < 0 && diff_x <= -d_x) )
            return (*m_initiators[2])->transport_dbg(trans);
        else if ( (diff_x > 0 && diff_x >= d_x) || (diff_x < 0 && diff_x > -d_x) )
            return (*m_initiators[0])->transport_dbg(trans);
        else
            return 0;
    } else {
        if (diff_y > 0)
            return (*m_initiators[1])->transport_dbg(trans);
        else if (diff_y < 0)
            return (*m_initiators[3])->transport_dbg(trans);
        else if (diff_x > 0)
            return (*m_initiators[2])->transport_dbg(trans);
        else if (diff_x < 0)
            return (*m_initiators[0])->transport_dbg(trans);
        else
            return 0;
    }
    return 1;
}


