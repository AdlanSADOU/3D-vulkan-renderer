
void AnimationUpdate(const Mesh *model, float dt)
{

    if (!model->_should_play_animation) return;

    if (!model->_current_animation) {
        SDL_Log("CurrentAnimation not set");
        return;
    }

    auto anim = model->_current_animation;

    assert(anim->duration > 0);
    anim->isPlaying = true;

    anim->globalTimer += dt;
    float animTime = fmodf(anim->globalTimer, anim->duration);


    static int iterationNb = 0;

    if (iterationNb++ == 0) {
        SDL_Log("playing:[%s] | duration: %fsec", ((cgltf_animation *)anim->handle)->name, anim->duration);
    }

    // For each Joint
    int channel_idx = 0;
    for (size_t joint_idx = 0; joint_idx < anim->_joints.size(); joint_idx++) {

        auto channels   = ((cgltf_animation *)anim->handle)->channels;
        int  currentKey = -1;
        int  nextKey    = -1;

        auto *sampler = channels[channel_idx].sampler;
        for (size_t timestamp_idx = 0; timestamp_idx < sampler->input->count - 1; timestamp_idx++) {
            float sampled_time = readFloatFromAccessor(sampler->input, timestamp_idx + 1);
            float sampled_time_prev;

            if (sampled_time > animTime) {
                currentKey = timestamp_idx;
                nextKey    = currentKey + 1;
                break;
            }
        }


        Transform currentPoseTransform;

        if (channels[channel_idx].target_path == cgltf_animation_path_type_translation) {
            currentPoseTransform.translation = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
            ++channel_idx;
        }
        if (channels[channel_idx].target_path == cgltf_animation_path_type_rotation) {
            currentPoseTransform.rotation = getQuatAtKeyframe(channels[channel_idx].sampler, currentKey);
            ++channel_idx;
        }
        if (channels[channel_idx].target_path == cgltf_animation_path_type_scale) {
            currentPoseTransform.scale = getVec3AtKeyframe(channels[channel_idx].sampler, currentKey);
            ++channel_idx;
        }

        auto current_pose_matrix = currentPoseTransform.GetLocalMatrix();
        auto parent_idx          = anim->_joints[joint_idx].parent;

        // comppute global matrix for current joint
        if (parent_idx == -1 && joint_idx > 0)
            anim->_joints[joint_idx].global_joint_matrix = current_pose_matrix;
        else if (joint_idx == 0)
            anim->_joints[0].global_joint_matrix = current_pose_matrix;
        else
            anim->_joints[joint_idx].global_joint_matrix = anim->_joints[parent_idx].global_joint_matrix * current_pose_matrix;

        anim->joint_matrices[joint_idx] = anim->_joints[joint_idx].global_joint_matrix * anim->_joints[joint_idx].inv_bind_matrix;
    } // End of Joints loop
}