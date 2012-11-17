#ifndef OCTREE_H
#define OCTREE_H

#include <map>
#include <stdexcept>

#include <tr1/memory>

#include "../generic/managed.h"
#include "kazmath/aabb.h"

/*
 * BANTER FOLLOWS!
 *
 * So, Octrees are cool. Our one needs to be dynamic and loose. This means that:
 *
 * 1. If an object is added outside the root node, we need to adjust for it. The steps in this case are:
 *
 * while new_object.outside(root):
 *  new_root = OctreeNode(width=root.width*2, position=root.position - direction_of_object)
 *  new_root.add_child(root)
 *  root = new_root
 * Basically, we dynamically grow upwards towards the object.
 *
 * 2. Each node of the octree should be loose. Which means that the bounds of the node are half the size
 * of the parent, rather than a quarter of the size.
 */


enum OctreePosition {
    NEGX_POSY_NEGZ,
    POSX_POSY_NEGZ,
    POSX_POSY_POSZ,
    NEGX_POSY_POSZ,
    NEGX_NEGY_NEGZ,
    POSX_NEGY_NEGZ,
    POSX_NEGY_POSZ,
    NEGX_NEGY_POSZ
};

class ChildNodeDoesNotExist :
    public std::logic_error {

    ChildNodeDoesNotExist():
        std::logic_error("Attempted to get a child node that doesn't exist") {}
};

class Boundable {
public:
    virtual const kmAABB absolute_bounds() const = 0;
    virtual const kmAABB local_bounds() const = 0;
    virtual const kmVec3 centre() const = 0;

    virtual void set_bounds(float width, float height, float depth) = 0;
    virtual void set_centre(const kmVec3& centre) = 0;
};

class Octree;

class OctreeNode :
    public Managed<OctreeNode> {

public:
    OctreeNode(OctreeNode* parent, float strict_diameter, const kmVec3 &centre);

    const kmVec3& centre() const {
        return centre_;
    }

    float width() const;
    float loose_width() const;

    uint8_t child_count() const { return children_.size(); }
    uint32_t object_count() const;

    OctreeNode& child(OctreePosition pos);
    bool has_child(OctreePosition pos) const;
    bool has_objects() const;

    bool is_root() const { return !parent_; }

    const kmAABB& absolute_loose_bounds() const;
    const kmAABB& absolute_strict_bounds() const;
    const float loose_diameter() const {
        //Any dimension will do...
        return kmAABBDiameterX(&loose_bounds_);
    }
    const float strict_diameter() const {
        //Any dimension will do...
        return kmAABBDiameterX(&strict_bounds_);
    }

private:
    OctreeNode* parent_;
    std::map<OctreePosition, std::tr1::shared_ptr<OctreeNode> > children_;

    kmAABB strict_bounds_;
    kmAABB loose_bounds_;
    kmVec3 centre_;

    void create_child(OctreePosition pos);

    OctreeNode& insert_into_subtree(const Boundable* obj);

    void add_object(const Boundable* obj);
    void remove_object(const Boundable* obj);

    friend class Octree;

};


/* A dynamic, loose Octree implementation. Things to note:
 *
 * * The tree can grow upwards and downwards as needed. Calling grow(Boundable*) will determine
 *   if nodes need to be added above or below the current tree
 * * In the event that an object exists in the cross-over between nodes, the one containing the
 *   centre point within it's "strict" bounds will take it.
 * * When an object changes location and moves outside its node, the tree will be recursed upwards
 *   until a parent is found that contains the object, and then recurse downwards to find the new
 *   node. If the object moves outside the root node this will require the tree to grow upwards. It
 *   is slower to move an object a great distance than a small one.
 * * Objects will be stored as far down the tree as they will fit
 * * If an object is added outside the bounds of the root node, the tree will grow in the direction
 *   of the new object until the root node encompasses it. This means that the Octree can move around in
 *   space. For example a fleet of spaceships moving a great distance will cause the root node to grow
 *   large, and then as child nodes empty shift in the direction of the fleet.
 * * If a node has no objects, and no children, it is removed. If the root node has no objects and only
 *   one child, then the child becomes the new root.
 */

class Octree {
public:
    Octree();

    OctreeNode& root() {
        if(!root_) {
            throw std::logic_error("Octree has not been initialized");
        }
        return *root_;
    }

    uint32_t node_count() const;

    void grow(const Boundable* object);
    void shrink(const Boundable* object);
    void relocate(const Boundable* object);

    OctreeNode& find(const Boundable* object);

private:
    OctreeNode::ptr root_;
    uint32_t node_count_;

    void _increment_node_count();
    void _decrement_node_count();

    void _register_object(const Boundable* obj);
    void _unregister_object(const Boundable* obj);

    std::map<const Boundable*, OctreeNode*> object_node_lookup_;

    friend class OctreeNode;
};


#endif // OCTREE_H
