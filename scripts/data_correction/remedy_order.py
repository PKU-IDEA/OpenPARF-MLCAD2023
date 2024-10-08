# %%
import os.path as osp
import os

current_directory = os.getcwd()
subdirectories = next(os.walk(current_directory))[1]

# remove nets with only one pin
# %%
for subdir in subdirectories:
    file_path = osp.join(subdir, "design.cascade_shape_instances")
    bk_path = osp.join(subdir, "design.cascade_shape_instances.remedy_order.bk")

    if not osp.exists(file_path):
        continue

    os.system("cp {0} {1}".format(file_path, bk_path))

    with open(file_path, "r") as f:
        lines = f.readlines()

    flag = 0
    shape_size = None
    for i, line in enumerate(lines):
        if line.startswith("BEGIN"):
            flag = 1
            shape_size = 0
            continue
        if line.startswith("END"):
            flag = 0
            shape_lines = lines[i - shape_size : i].copy()
            sorted_lines = sorted(
                shape_lines, key=lambda x: int("".join(filter(str.isdigit, x)))
            )
            lines[i - shape_size : i] = sorted_lines
            continue
        if flag == 1:
            shape_size += 1
    # %%
    with open(file_path, "w") as f:
        f.writelines(lines)
    print("Done {0}".format(subdir))
    # %%
