/* Genesis3D Portal Implementation - To be integrated into mapscene_bsp.c */

// ============================================================================
// PORTAL MANAGEMENT (Genesis3D Style)
// ============================================================================

static BSPPortal *
AllocPortal_G3D(void)
{
	BSPPortal *portal;

	portal = (BSPPortal*)malloc(sizeof(BSPPortal));
	if (!portal)
		return NULL;
	
	memset(portal, 0, sizeof(BSPPortal));
	return portal;
}

static void
FreePortal_G3D(BSPPortal *portal)
{
	if (!portal)
		return;
	
	if (portal->winding)
		Winding_Free(portal->winding);
	
	free(portal);
}

static bool
AddPortalToNodes(BSPPortal *portal, void *front_node, void *back_node, 
                 bool front_is_leaf, bool back_is_leaf)
{
	if (portal->nodes[0] || portal->nodes[1]) {
		printf("ERROR: Portal already linked to nodes\n");
		return false;
	}
	
	portal->nodes[0] = front_node;
	portal->nodes[1] = back_node;
	
	/* Add to front node's portal list */
	if (front_is_leaf) {
		BSPLeaf *leaf = (BSPLeaf*)front_node;
		portal->next[0] = leaf->portals;
		leaf->portals = portal;
	} else {
		BSPNode *node = (BSPNode*)front_node;
		portal->next[0] = node->portals;
		node->portals = portal;
	}
	
	/* Add to back node's portal list */
	if (back_is_leaf) {
		BSPLeaf *leaf = (BSPLeaf*)back_node;
		portal->next[1] = leaf->portals;
		leaf->portals = portal;
	} else {
		BSPNode *node = (BSPNode*)back_node;
		portal->next[1] = node->portals;
		node->portals = portal;
	}
	
	return true;
}

static bool
RemovePortalFromNode(BSPPortal *portal, void *node, bool is_leaf, int side)
{
	BSPPortal **pp, *p;
	int s;
	
	/* Find portal in node's list */
	if (is_leaf) {
		BSPLeaf *leaf = (BSPLeaf*)node;
		for (pp = &leaf->portals, p = *pp; p; pp = &p->next[s], p = *pp) {
			s = (p->nodes[1] == node);
			if (p == portal) {
				*pp = portal->next[side];
				portal->nodes[side] = NULL;
				return true;
			}
		}
	} else {
		BSPNode *n = (BSPNode*)node;
		for (pp = &n->portals, p = *pp; p; pp = &p->next[s], p = *pp) {
			s = (p->nodes[1] == node);
			if (p == portal) {
				*pp = portal->next[side];
				portal->nodes[side] = NULL;
				return true;
			}
		}
	}
	
	printf("ERROR: Portal not found in node's list\n");
	return false;
}

// ============================================================================
// PORTAL CREATION - CreatePolyOnNode (Genesis3D)
// ============================================================================

static winding_t *
CreatePolyOnNode(BSPTree *tree, int node_idx)
{
	BSPNode *node, *parent;
	winding_t *w;
	Vector3 plane_normal;
	float plane_dist;
	bool side;
	int parent_idx;
	
	if (node_idx < 0 || node_idx >= tree->node_count)
		return NULL;
	
	node = &tree->nodes[node_idx];
	
	/* Create huge poly on this node's plane */
	w = Winding_FromPlane(node->plane_normal, node->plane_dist);
	if (!w)
		return NULL;
	
	/* Clip by all parent planes walking up the tree */
	/* TODO: Track parent in BSPNode, or find parent by searching */
	
	return w;
}

// ============================================================================
// PARTITION PORTALS (Genesis3D Algorithm)
// ============================================================================

static bool
PartitionPortals_r(BSPTree *tree, int node_idx)
{
	BSPNode *node, *front, *back;
	BSPPortal *portal, *next_portal, *new_portal;
	winding_t *node_winding, *front_w, *back_w;
	int side, front_child, back_child;
	bool front_is_leaf, back_is_leaf;
	
	if (node_idx < 0)
		return true; /* Leaf - done */
	
	if (node_idx >= tree->node_count) {
		printf("ERROR: Invalid node index %d\n", node_idx);
		return false;
	}
	
	node = &tree->nodes[node_idx];
	
	front_child = node->front_child;
	back_child = node->back_child;
	front_is_leaf = node->front_is_leaf;
	back_is_leaf = node->back_is_leaf;
	
	/* Create portal on this node's plane */
	node_winding = CreatePolyOnNode(tree, node_idx);
	
	/* Clip portal by all portals already on this node */
	for (portal = node->portals; portal && node_winding; ) {
		/* Determine which side of portal we're on */
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
			side = 1;
		else {
			printf("ERROR: Portal doesn't reference this node\n");
			return false;
		}
		
		portal = portal->next[side];
		
		/* Clip node_winding by this portal's plane */
		/* TODO: Implement clipping */
	}
	
	/* If winding survived, create portal between children */
	if (node_winding && !Winding_IsTiny(node_winding)) {
		new_portal = AllocPortal_G3D();
		if (!new_portal) {
			Winding_Free(node_winding);
			return false;
		}
		
		new_portal->winding = node_winding;
		new_portal->plane_normal = node->plane_normal;
		new_portal->plane_dist = node->plane_dist;
		new_portal->on_node = node;
		
		/* Get pointers to children */
		void *front_ptr, *back_ptr;
		if (front_is_leaf)
			front_ptr = &tree->leaves[-(front_child - OUTSIDE_NODE)];
		else
			front_ptr = &tree->nodes[front_child];
		
		if (back_is_leaf)
			back_ptr = &tree->leaves[-(back_child - OUTSIDE_NODE)];
		else
			back_ptr = &tree->nodes[back_child];
		
		AddPortalToNodes(new_portal, front_ptr, back_ptr, 
		                front_is_leaf, back_is_leaf);
	} else if (node_winding) {
		Winding_Free(node_winding);
	}
	
	/* Partition all portals on this node to children */
	for (portal = node->portals; portal; portal = next_portal) {
		/* Determine side */
		if (portal->nodes[0] == node)
			side = 0;
		else
			side = 1;
		
		next_portal = portal->next[side];
		
		/* Split portal winding */
		Winding_Split(portal->winding, node->plane_normal, node->plane_dist,
		             &front_w, &back_w);
		
		/* TODO: Remove from current nodes, create new portals for children */
	}
	
	/* Recurse to children */
	if (!front_is_leaf) {
		if (!PartitionPortals_r(tree, front_child))
			return false;
	}
	
	if (!back_is_leaf) {
		if (!PartitionPortals_r(tree, back_child))
			return false;
	}
	
	return true;
}
