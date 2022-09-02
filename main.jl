
function TwoJointsSingleInfluence()
    # bind pose ------------
    root_bindPose = [
        cos(0) -sin(0) 0
        sin(0) cos(0) 2
        0 0 1
    ]

    j1_bindPose = [
        cos(0) -sin(0) 0
        sin(0) cos(0) 3
        0 0 1
    ]
    j1_invBindMatrix = inv(root_bindPose * j1_bindPose)
    # frame 1 --------------

    root_f1 = [
        cos(deg2rad(-40)) -sin(deg2rad(-40)) 0
        sin(deg2rad(-40)) cos(deg2rad(-40)) 2
        0 0 1
    ]

    j1_f1 = [
        cos(deg2rad(-60)) -sin(deg2rad(-60)) 0
        sin(deg2rad(-60)) cos(deg2rad(-60)) 3
        0 0 1
    ]


    # model
    M = [
        cos(0) -sin(0) 0
        sin(0) cos(0) 2
        0 0 1
    ]

    vPos = [0, 6, 1]

    # since out vertex is only influenced by 1 joint
    # we do not need the globalTransform
    globalTransform = root_bindPose


    f1_globalJointTransform = root_f1 * j1_f1
    jointMatrix = f1_globalJointTransform * j1_invBindMatrix
    modelSpace_Vpos = jointMatrix * vPos

    display(modelSpace_Vpos)
end

function TwoJoints2Influences()
    # bind pose ------------
    root_bindPose = [
        cos(0) -sin(0) 0
        sin(0) cos(0) 2
        0 0 1
    ]

    j1_bindPose = [
        cos(0) -sin(0) 0
        sin(0) cos(0) 3
        0 0 1
    ]
    j1_invBindMatrix = inv(root_bindPose * j1_bindPose)
    # frame 1 --------------

    root_f1 = [
        cos(deg2rad(-40)) -sin(deg2rad(-40)) 0
        sin(deg2rad(-40)) cos(deg2rad(-40)) 2
        0 0 1
    ]

    j1_f1 = [
        cos(deg2rad(-60)) -sin(deg2rad(-60)) 0
        sin(deg2rad(-60)) cos(deg2rad(-60)) 3
        0 0 1
    ]

    # model
    M = [
        cos(0) -sin(0) 0
        sin(0) cos(0) 2
        0 0 1
    ]

    vPos = [0, 6, 1]
    w0 = 0.5
    w1 = 0.5
    globalTransform = root_bindPose

    f1_globalJointTransform = root_f1 * j1_f1

    jointMatrix0 = inv(globalTransform) * root_f1 * inv(root_bindPose)
    jointMatrix1 = inv(globalTransform) * f1_globalJointTransform * j1_invBindMatrix

    skinnedMatrix =
        w0 * jointMatrix0 +
        w1 * jointMatrix1

    display(globalTransform * skinnedMatrix * vPos)

end


# TwoJointsSingleInfluence()
TwoJoints2Influences()