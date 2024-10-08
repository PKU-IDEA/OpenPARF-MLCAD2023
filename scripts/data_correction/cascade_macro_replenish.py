# %%
import os.path as osp
import os

#%%
current_directory = os.getcwd()
subdirectories = next(os.walk(current_directory))[1]

#%%
for subdir in subdirectories:
    file_path = osp.join(subdir, "design.cascade_shape_instances")
    bk_path = osp.join(subdir, "design.cascade_shape_instances.cascade_macro_replenish.bk")

    if not osp.exists(file_path):
        continue

    os.system("cp {0} {1}".format(file_path, bk_path))

    with open(file_path, "r") as f:
        lines = f.readlines()

    flag = 0
    expected_shape_size = None
    shape_size = None
    newlines = []
    for i, line in enumerate(lines):
        if line.startswith("BEGIN"):
            flag = 1
            shape_size = 0
            expected_shape_size = int(lines[i-1].strip().split()[1])
            newlines.append(line)
            continue
        if line.startswith("END"):
            flag = 0
            if shape_size != expected_shape_size:
                pos = lines[i-shape_size].index("name1")
                a, b = lines[i-shape_size].split("name1")
                for j in range(expected_shape_size):
                    newlines.append(a+"name"+str(j+1)+b)
            else:
                for x in lines[i - shape_size:i]:
                    newlines.append(x)
            newlines.append(line)
            continue
        if flag == 1:
            shape_size += 1
        else:
            newlines.append(line)

    with open(file_path, "w") as f:
        f.writelines(newlines)
    print("Done {0}".format(subdir))